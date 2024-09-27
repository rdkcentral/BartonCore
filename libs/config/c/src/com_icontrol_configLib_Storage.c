//
// Created by gfaulk200 on 7/27/20.
//
#ifdef CONFIG_OS_ANDROID
#include "icConfig/com_icontrol_configLib_Storage.h"
#include "icConfig/storage.h"

JNIEXPORT jboolean JNICALL Java_com_icontrol_configLib_Storage_save(JNIEnv *env, jclass clazz, jstring jnamespace, jstring jkey, jstring jvalue)
{
    const jchar *namespace = NULL;
    jboolean isCopy;
    const jchar *key = NULL;
    const jchar *value = NULL;
    jboolean retVal = JNI_FALSE;

    namespace = (jchar *)((*env)->GetStringUTFChars(env, jnamespace, &isCopy));
    key = (jchar *)((*env)->GetStringUTFChars(env, jkey, &isCopy));
    value = (jchar *)((*env)->GetStringUTFChars(env, jvalue, &isCopy));

    if ((namespace != NULL) && (key != NULL) && (value != NULL))
    {
        bool result = storageSave((const char *)namespace, (const char *) key, (const char *)value);
        if (result == true)
        {
            retVal = JNI_TRUE;
        }
    }

    if (namespace != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jnamespace, (const char *)namespace);
    }
    if (key != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jkey, (const char *)key);
    }
    if (value != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jvalue, (const char *)value);
    }
    return retVal;
}

JNIEXPORT jstring JNICALL Java_com_icontrol_configLib_Storage_load(JNIEnv *env, jclass clazz, jstring jnamespace, jstring jkey)
{
    const jchar *namespace = NULL;
    jboolean isCopy;
    const jchar *key = NULL;
    jstring retVal = NULL;
    namespace = (jchar *)((*env)->GetStringUTFChars(env, jnamespace, &isCopy));
    key = (jchar *)((*env)->GetStringUTFChars(env, jkey, &isCopy));

    if ((namespace != NULL) && (key != NULL))
    {
        char *value = NULL;
        bool result = storageLoad((const char *)namespace, (const char *)key, &value);
        if ((result == true) && (value != NULL))
        {
            retVal = (*env)->NewStringUTF(env, value);
            free(value);
        }
    }

    if (namespace != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jnamespace, (const char *)namespace);
    }
    if (key != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jkey, (const char *)key);
    }

    return retVal;
}

JNIEXPORT jboolean JNICALL Java_com_icontrol_configLib_Storage_delete(JNIEnv *env, jclass clazz, jstring jnamespace, jstring jkey)
{
    const jchar *namespace = NULL;
    jboolean isCopy;
    const jchar *key = NULL;
    jboolean retVal = JNI_FALSE;

    namespace = (jchar *)((*env)->GetStringUTFChars(env, jnamespace, &isCopy));
    key = (jchar *)((*env)->GetStringUTFChars(env, jkey, &isCopy));

    if ((namespace != NULL) && (key != NULL))
    {
        bool result = storageDelete((const char *)namespace, (const char *)key);
        if (result == true)
        {
            retVal = JNI_TRUE;
        }
    }

    if (namespace != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jnamespace, (const char *)namespace);
    }
    if (key != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, jkey, (const char *)key);
    }

    return retVal;
}

JNIEXPORT jboolean JNICALL Java_com_icontrol_configLib_Storage_deleteNamespace(JNIEnv *env, jclass clazz, jstring jnamespace)
{
    const jchar *namespace = NULL;
    jboolean isCopy;
    jboolean retVal = JNI_FALSE;

    namespace = (jchar *)((*env)->GetStringUTFChars(env, jnamespace, &isCopy));
    if (namespace != NULL)
    {
        bool result = storageDeleteNamespace((const char *)namespace);
        if (result == true)
        {
            retVal = JNI_TRUE;
        }
        (*env)->ReleaseStringUTFChars(env, jnamespace, (const char *)namespace);
    }

    return retVal;
}
#endif
