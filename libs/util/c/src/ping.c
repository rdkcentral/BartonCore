//
// Created by wboyd747 on 6/17/20.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef CONFIG_OS_ANDROID
#include <linux/icmp.h>
#endif

#include <netinet/ip_icmp.h>

#include <icUtil/ping.h>


#define IPHDR_SIZE       sizeof(struct iphdr)
#define ICMPHDR_SIZE     sizeof(struct icmphdr)

/*
 * The IP headers IHL (header length) can hold a maximum of 60B.
 * This represents the 20B header itself plus "data" tacked after
 * the IP header.
 */
#define MAX_IP_SIZE      60
#define MAX_DATA_SIZE    0 // Currently we are not going to support data in the ICMP

#define PING_PACKET_SIZE (MAX_IP_SIZE + ICMPHDR_SIZE + MAX_DATA_SIZE)

/**
 * The checksum here is pulled from the Net.
 *
 * @param src Source buffer to be computed against
 * @param len The number of bytes to use.
 * @return The final checksum value of the source.
 */
static uint16_t checksum(const void *src, int len)
{
    const uint16_t *buffer = src;
    uint32_t sum;

    for (sum = 0; len > 1; len -= 2)
    {
        sum += *buffer++;
    }

    if (len == 1)
    {
        sum += *((uint8_t *) buffer);
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return ~sum;
}

int create_socket(const char *iface, uint32_t fwmark, int ttl, int packetTimeoutMs, int *type)
{
    int fd;

    struct timeval tval;
    int __type = (type == NULL) ? SOCK_RAW : *type;

    if ((__type != SOCK_DGRAM) && (__type != SOCK_RAW))
    {
        errno = EINVAL;
        return -1;
    }

    fd = socket(AF_INET, __type | SOCK_CLOEXEC, IPPROTO_ICMP);
    if (fd < 0)
    {
        if (__type == SOCK_DGRAM)
        {
            __type = SOCK_RAW;

            fd = socket(AF_INET, __type | SOCK_CLOEXEC, IPPROTO_ICMP);
            if (fd < 0)
            {
                return -1;
            }
        }
        else
        {
            // Bail out as RAW socket is not supported.
            return -1;
        }
    }

    if (type != NULL)
    {
        *type = __type;
    }

    /*
     * Default to a rather large TTL if one is not provided.
     */
    if (ttl < 1)
    {
        ttl = 64;
    }

    if (setsockopt(fd, SOL_IP, IP_TTL, &ttl, sizeof(int)) != 0)
    {
        close(fd);
        return -1;
    }

    if (fwmark > 0)
    {
        if (setsockopt(fd, SOL_SOCKET, SO_MARK, &fwmark, sizeof(int)) != 0)
        {
            close(fd);
            return -1;
        }
    }

    if ((iface != NULL) && (iface[0] != '\0'))
    {
        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) + 1) != 0)
        {
            close(fd);
            return -1;
        }
    }

    if (packetTimeoutMs >= 0)
    {
        tval.tv_sec = packetTimeoutMs / 1000;
        tval.tv_usec = (packetTimeoutMs % 1000) * 1000;

        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const void *) &tval, sizeof(struct timeval)) != 0)
        {
            close(fd);
            return -1;
        }
    }

    /*
     * Set a very low send timeout so that we do not get stuck
     * waiting for any of the send buffer to be empty. Fact
     * is we are transmitting so little data and over such a
     * large time frame that we should never hit the need for
     * this. It is just a safety net to make sure we do not
     * block anything up.
     */
    tval.tv_sec = 0;
    tval.tv_usec = 5000;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const void *) &tval, sizeof(struct timeval)) != 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

int icPing(const char *iface,
           const struct sockaddr *dst,
           uint32_t count,
           uint32_t fwmark,
           int ttl,
           int packetTimeoutMs,
           struct pingResults *results)
{
    int id, socket_type, sockfd;

    struct pingResults internal_results;

    /*
     * No need to do anything if the count is zero.
     */
    if (count == 0)
    {
        return 0;
    }

    if (dst == NULL)
    {
        return EINVAL;
    }

    /*
     * There is a whole separate ICMPv6 for
     * IPv6. Thus many of the ping applications require
     * you to specify that you want IPv6. In this case
     * we left the `struct sockaddr` ambiguous so that
     * we could one day support IPv6. For now though
     * just let the caller know we cannot help them.
     */
    if (dst->sa_family != AF_INET)
    {
        return EPFNOSUPPORT;
    }

    memset(&internal_results, 0, sizeof(struct pingResults));
    internal_results.rtt_min_usec = UINT32_MAX;

    srandom(time(NULL));
    id = random() & 0xFFFF;

    socket_type = SOCK_DGRAM;

    if ((sockfd = create_socket(iface, fwmark, ttl, packetTimeoutMs, &socket_type)) < 0)
    {
        return errno;
    }

    for (int i = 0; i < count; i++)
    {
        uint8_t buffer[PING_PACKET_SIZE];

        struct icmphdr *icmp;
        struct timespec t0;

        memset(buffer, 0, PING_PACKET_SIZE);

        icmp = (struct icmphdr *) buffer;
        icmp->type = ICMP_ECHO;
        icmp->un.echo.id = id;
        icmp->un.echo.sequence = i;
        icmp->checksum = checksum(buffer, (ICMPHDR_SIZE + MAX_DATA_SIZE));

        clock_gettime(CLOCK_MONOTONIC, &t0);

        /*
         * The kernel will apply the IP header on the send so
         * we do not have to manage that portion in the
         * buffer. Thus we can focus just on the ICMP header
         * and any data we wish to send.
         */
        if (sendto(sockfd, buffer, (ICMPHDR_SIZE + MAX_DATA_SIZE), 0, dst, sizeof(struct sockaddr_in)) > 0)
        {
            struct sockaddr_storage recv_addr_buffer;
            socklen_t recv_len = sizeof(struct sockaddr_storage);
            int bytes_read;

            internal_results.transmitted++;

            /*
             * For SOCK_RAW the receive call _will_ have the IP header as part
             * of the data returned. Thus we must make sure
             * and move past the IP header and any of its data.
             *
             * For SOCK_DGRAM the data will be only the ICMP response.
             */
            if ((bytes_read = recvfrom(
                     sockfd, buffer, PING_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr_buffer, &recv_len)) > 0)
            {
                struct timespec t1;
                int min_bytes_read = ICMPHDR_SIZE;

                clock_gettime(CLOCK_MONOTONIC, &t1);

                /*
                 * RAW sockets have the IP header attached from the
                 * read. DGRAM packets do not.
                 */
                if (socket_type == SOCK_RAW)
                {
                    min_bytes_read += IPHDR_SIZE;
                }

                if (bytes_read >= min_bytes_read)
                {
                    if (socket_type == SOCK_RAW)
                    {
                        struct iphdr *iphdr = (struct iphdr *) buffer;

                        /*
                         * Move past the IP header and any of its extra "data".
                         */
                        // coverity[tainted_data] IHL saturates at 15 * (32 / 8) = 60 bytes. PING_PACKET_SIZE Is larger
                        // than this.
                        icmp = (struct icmphdr *) &buffer[(unsigned int) iphdr->ihl << 2U];
                    }
                    else
                    {
                        icmp = (struct icmphdr *) buffer;
                    }

                    /*
                     * DGRAM sockets reuse the ID field in the ICMP to point
                     * to the local port assigned to the connection. Thus
                     * there is no need to verify ID as it is done by the
                     * kernel, and in fact we cannot verify it as we do not
                     * know which port the kernel chose.
                     */
                    if ((icmp->type == ICMP_ECHOREPLY) && ((socket_type == SOCK_DGRAM) || (icmp->un.echo.id == id)) &&
                        (icmp->un.echo.sequence == i))
                    {
                        uint32_t us;

                        us = (t1.tv_sec - t0.tv_sec) * 1000000UL;

                        if (t1.tv_nsec < t0.tv_nsec)
                        {
                            us -= 1000000; // Remove the 1s rollover
                            us += (t1.tv_nsec - t0.tv_nsec + 1000000000L) / 1000UL;
                        }
                        else
                        {
                            us += (t1.tv_nsec - t0.tv_nsec) / 1000UL;
                        }

                        internal_results.received++;
                        internal_results.rtt_avg_usec += us;
                        internal_results.rtt_max_usec =
                            (us > internal_results.rtt_max_usec) ? us : internal_results.rtt_max_usec;
                        internal_results.rtt_min_usec =
                            (us < internal_results.rtt_min_usec) ? us : internal_results.rtt_min_usec;
                    }
                }
            }
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    if (results)
    {
        /*
         * Prevent divide by zero.
         */
        if (internal_results.received > 0)
        {
            internal_results.rtt_avg_usec /= internal_results.received;
        }

        memcpy(results, &internal_results, sizeof(struct pingResults));
    }

    /*
     * If we do not receive any packets back let the
     * caller know that the endpoint was unreachable.
     */
    return (internal_results.received > 0) ? 0 : EHOSTUNREACH;
}
