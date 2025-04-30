//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Micah Koch on 11/08/24.
 *
 * The basic idea here is to utilize a multiplexing I/O approach for managing the prompt and other async output.
 * We have an I/O thread that is waiting on input from the user, output from logs, and output from commands. When we
 * receive output or logs we can manipulate the prompt in a such a way to keep the output from mangling the prompt/user
 * input and vice versa. linenoise provides an API for this approach, and we do our best in the case we aren't using
 * linenoise.  The below diagram illustrates the approach:
 *
 *              --------------            ---------------          --------------         ----------------------
 *              | User Input | ---------- | Pipe Output | <------- | Pipe Input | <-------| Log/Command Output |
 *              --------------      |     ---------------          --------------         ----------------------
 *                                  v
 *                          select for reading
 *                                  |
 *   process input <- user input ---+--- pipe output -> print to terminal and reset prompt
 */

#include "device-service-reference-io.h"
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <glib.h>
#include <icConcurrent/threadUtils.h>
#include <icLog/logging.h>
#include <linenoise.h>

#define PROMPT "deviceService> "

static pthread_once_t init = PTHREAD_ONCE_INIT;
static pthread_mutex_t mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static int outputReceivePipe;
static int outputSendPipe;

typedef struct ioAsyncContext
{
    processLineCallback callback;
    bool useLinenoise;
    struct linenoiseState ls;
    char buf[1024];
    int bufIndex;
    void *userData;
    const GMainLoop *mainLoop;
    bool showingPrompt;  // Used for state when not using linenoise
    GTimer *promptTimer; // Used for state when not using linenoise
} ioAsyncContext;

static void ioShutdown(void)
{
    mutexLock(&mtx);
    int localOutputReceivePipe = outputReceivePipe;
    int localOutputSendPipe = outputSendPipe;
    outputReceivePipe = 0;
    outputSendPipe = 0;
    mutexUnlock(&mtx);
    if (localOutputReceivePipe != 0)
    {
        close(localOutputReceivePipe);
    }
    if (localOutputSendPipe != 0)
    {
        close(localOutputSendPipe);
    }
}

static void ioSetup(void)
{
    mutexLock(&mtx);
    if (outputReceivePipe == 0)
    {
        int fds[2];
        if (pipe(fds) == 0)
        {
            outputReceivePipe = fds[0];
            outputSendPipe = fds[1];
        }
        atexit(ioShutdown);
    }
    mutexUnlock(&mtx);
}

// Logger hook so tha we can control when stuff is output to the terminal
int getDebugLoggerFileDescriptor(void)
{
    pthread_once(&init, ioSetup);
    LOCK_SCOPE(mtx);
    return outputSendPipe;
}

static bool linenoiseStartRead(ioAsyncContext *asyncContext)
{
    return linenoiseEditStart(&asyncContext->ls, -1, -1, asyncContext->buf, sizeof(asyncContext->buf), PROMPT) != -1;
}

static void outputLineReady(GObject *inputStream, GAsyncResult *res, ioAsyncContext *data)
{
    g_autoptr(GError) error = NULL;
    GDataInputStream *dataInputStream = G_DATA_INPUT_STREAM(inputStream);
    gsize size = 0;
    g_autofree gchar *line = g_data_input_stream_read_line_finish_utf8(dataInputStream, res, &size, &error);
    if (line != NULL)
    {
        // Got some output so to print so take care of sending it out to the terminal while also manipulating the prompt
        // so as to not interfere

        if (data->useLinenoise)
        {
            linenoiseHide(&data->ls);
        }

        if (data->useLinenoise)
        {
            printf("%s\n", line);
            linenoiseShow(&data->ls);
        }
        else
        {
            if (data->showingPrompt)
            {
                printf("\n%s\n", line);
            }
            else
            {
                printf("%s\n", line);
            }
            data->showingPrompt = false;
            GInputStream *baseInputStream =
                g_filter_input_stream_get_base_stream(G_FILTER_INPUT_STREAM(dataInputStream));
            GPollableInputStream *pollableStream = G_POLLABLE_INPUT_STREAM(baseInputStream);
            if (!g_pollable_input_stream_is_readable(pollableStream))
            {
                g_timer_stop(data->promptTimer);
                g_timer_start(data->promptTimer);
            }
        }
        g_data_input_stream_read_line_async(
            dataInputStream, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback) outputLineReady, data);
    }
    else
    {
        fprintf(stderr, "Error reading line: %s", error->message);
    }
}

static gboolean linenoiseInputReady(GObject *pollableStream, ioAsyncContext *data)
{
    gboolean retVal = G_SOURCE_CONTINUE;

    // Sanity check to ensure there is actually something to read
    if (g_pollable_input_stream_is_readable(G_POLLABLE_INPUT_STREAM(pollableStream)))
    {
        // Check if we have a full line or if we are still waiting for more input
        char *line = linenoiseEditFeed(&data->ls);
        if (line != linenoiseEditMore)
        {
            linenoiseEditStop(&data->ls);
            if (line == NULL)
            {
                // Ctrl-C/Ctrl-D
                retVal = G_SOURCE_REMOVE;
            }
            else if (g_ascii_strcasecmp(line, "") == 0)
            {
                // In GDB the prompt was printing indented if the user hit enter without typing anything.  Emitting a
                // new line ensures the prompt is at the left margin.
                emitOutput("\n");
            }
            if (!data->callback(line, data->userData))
            {
                // If the callback returns false it means we are done and should exit
                retVal = G_SOURCE_REMOVE;
            }
            if (retVal != G_SOURCE_REMOVE)
            {
                linenoiseStartRead(data);
            }
            else
            {
                // We are done, so this will force everything to stop so we stop blocking and return
                g_main_loop_quit((GMainLoop *) data->mainLoop);
            }
            free(line);
        }
    }

    return retVal;
}

static void inputReady(GObject *inputStream, GAsyncResult *res, ioAsyncContext *data)
{
    GInputStream *is = G_INPUT_STREAM(inputStream);
    g_autoptr(GError) err = NULL;
    gssize size = g_input_stream_read_finish(is, res, &err);
    if (size > 0)
    {
        if (data->buf[data->bufIndex] == '\n')
        {
            data->buf[data->bufIndex] = '\0';
            if (!data->callback(data->buf, data->userData))
            {
                g_main_loop_quit((GMainLoop *) data->mainLoop);
            }
            memset(data->buf, 0, sizeof(data->buf));
            data->bufIndex = 0;
        }
        else
        {
            ++data->bufIndex;
            if (data->bufIndex >= sizeof(data->buf))
            {
                fprintf(stderr, "\nError: Line too long\n");
                memset(data->buf, 0, sizeof(data->buf));
                data->bufIndex = 0;
            }
        }
        g_input_stream_read_async(
            is, &data->buf[data->bufIndex], 1, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback) inputReady, data);
    }
    else
    {
        if (err != NULL)
        {
            fprintf(stderr, "Error reading line: %s", err->message);
        }
        else
        {
            fprintf(stderr, "Error reading line");
        }

        g_main_loop_quit((GMainLoop *) data->mainLoop);
    }
}

static gboolean checkDisplayPrompt(ioAsyncContext *data)
{
    if (!data->showingPrompt && g_timer_elapsed(data->promptTimer, NULL) > .5)
    {
        printf(PROMPT "%s", data->buf);
        fflush(stdout);
        g_timer_stop(data->promptTimer);
        data->showingPrompt = true;
    }
    return G_SOURCE_CONTINUE;
}

static void destroyAsyncContext(ioAsyncContext *data)
{
    if (data)
    {
        if (!data->useLinenoise)
        {
            g_timer_destroy(data->promptTimer);
        }
    }
}

void device_service_reference_io_process(bool useLinenoise, processLineCallback callback, void *userData)
{
    pthread_once(&init, ioSetup);

    GMainLoop *loop = NULL;
    GMainContext *context = g_main_context_new();
    loop = g_main_loop_new(context, FALSE);
    g_main_context_push_thread_default(context);

    ioAsyncContext *asyncContext = g_atomic_rc_box_new0(ioAsyncContext);
    asyncContext->useLinenoise = useLinenoise;
    asyncContext->callback = callback;
    asyncContext->userData = userData;
    asyncContext->mainLoop = loop;
    mutexLock(&mtx);
    int localOutputReceivePipe = outputReceivePipe;
    mutexUnlock(&mtx);

    // Setup reading from the output fd
    g_autoptr(GDataInputStream) outputStream =
        g_data_input_stream_new(g_unix_input_stream_new(localOutputReceivePipe, FALSE));
    g_data_input_stream_read_line_async(
        outputStream, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback) outputLineReady, asyncContext);

    // Setup reading from stdin
    g_autoptr(GInputStream) inputStream = NULL;
    if (useLinenoise && linenoiseStartRead(asyncContext))
    {
        linenoiseStartRead(asyncContext);
        inputStream = g_unix_input_stream_new(asyncContext->ls.ifd, FALSE);
        GSource *source = g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(inputStream), NULL);
        g_source_set_name(source, "linenoise");
        g_source_set_priority(source, G_PRIORITY_DEFAULT);
        g_source_attach(source, context);
        g_source_set_callback(source, (GSourceFunc) linenoiseInputReady, asyncContext, NULL);
    }
    else
    {
        asyncContext->useLinenoise = false;
        printf(PROMPT);
        fflush(stdout);
        asyncContext->promptTimer = g_timer_new();
        asyncContext->showingPrompt = true;
        inputStream = g_unix_input_stream_new(STDIN_FILENO, false);
        g_input_stream_read_async(inputStream,
                                  asyncContext->buf,
                                  1,
                                  G_PRIORITY_DEFAULT,
                                  NULL,
                                  (GAsyncReadyCallback) inputReady,
                                  asyncContext);

        // Waits for a quiet period of output before re-displaying the prompt.  Will also attempt to display any input
        // the user had previously typed, but if in linemode this doesn't work.  But if you are in a tty with linemode
        // you should just be using the linenoise mode anyways.
        GSource *source = g_timeout_source_new_seconds(1);
        g_source_set_name(source, "promptTimer");
        g_source_set_priority(source, G_PRIORITY_DEFAULT);
        g_source_attach(source, context);
        g_source_set_callback(source, (GSourceFunc) checkDisplayPrompt, asyncContext, NULL);
    }

    // We will block until one of the async callbacks tells us to quit because the user chose to exit
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_atomic_rc_box_release_full(asyncContext, (GDestroyNotify) destroyAsyncContext);
}

static void emitToPipe(int fd, const gchar *str)
{
    if (fd > 0)
    {
        write(fd, str, strlen(str));
    }
}

void emitOutput(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    g_autofree gchar *str = g_strdup_vprintf(format, args);
    if (str)
    {
        // Could create another pipe pair and seperate logging and output.  This would be useful as it could give the
        // option of silencing the logging output.
        int localSendPipe = getDebugLoggerFileDescriptor();
        emitToPipe(localSendPipe, str);
    }
    va_end(args);
}

void emitError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    g_autofree gchar *str = g_strdup_vprintf(format, args);
    if (str)
    {
        // Could create another pipe pair and have it go to stderr
        int localSendPipe = getDebugLoggerFileDescriptor();
        emitToPipe(localSendPipe, str);
    }
    va_end(args);
}
