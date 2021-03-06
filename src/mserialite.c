/*
 *   Copyright 2013 Emeric Verschuur <emericv@openihs.org>
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *		   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <jni.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <org_mbedsys_io_SerialDriver.h>

#define VERSION "1.1.0"

#define BREAK_ON_FAIL(exp) if ((exp) == -1) break
#define DEBUG(args...) if (debug) fprintf (stderr, args)
#define DEBUG_ARRAY(array, off, len) if (debug) _dumpByteArray(array, off, len)

int debug = 0;

typedef struct {
	FILE *in;
	FILE *out;
} iostream_t;

void _dumpByteArray(void *array, int off, int len) {
	int i;
	for (i = off; i < len; i++) {
		fprintf(stderr, " %02X", ((char*)array)[i] & 0xFF);
	}
}

void _throwIOException(JNIEnv *env, const char *message) {
	jclass javaIoExcp = (*env)->FindClass(env, "java/io/IOException");
	if (javaIoExcp == NULL) {
		fprintf(stderr, " < CC > GNU IO Native lite serial driver: Class java.io.IOException not found!\n");
		return;
	}
	(*env)->ThrowNew(env, javaIoExcp, message);
}

void _throwIllegalArgumentException(JNIEnv *env, const char *message) {
	jclass javaIoExcp = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
	if (javaIoExcp == NULL) {
		fprintf(stderr, " < CC > GNU IO Native lite serial driver: Class java.lang.IllegalArgumentException not found!\n");
		return;
	}
	(*env)->ThrowNew(env, javaIoExcp, message);
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    setDebugEnabled
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mbedsys_io_SerialDriver_setDebugEnabled
  (JNIEnv *env, jclass klass, jboolean enabled) {
	debug = enabled;
	if (enabled) {
		fprintf (stderr, " < DD > GNU IO Native lite serial driver version "
		VERSION" (build date: "__DATE__" "__TIME__")\n < DD > GNU IO Native lite serial driver: Debug enabled\n");
	}
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _open
 * Signature: (Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_org_mbedsys_io_SerialDriver__1open
  (JNIEnv *env, jclass klass, jstring devNameId, jstring optionsId) {
	const char *devName = NULL;
	char *options = NULL;
	const char *error = NULL;
	char *key;
	char *val;
	char *next;
	char *parity = "none";
	int fd = -1;
	int ret = 0;
	int baudrate = 57600;
	int bitsperchar = 8;
	int stopbits = 1;
	int autocts = 1;
	int autorts = 1;
	int blocking = 1;
	struct termios config;
	do {
		if ((devName = (*env)->GetStringUTFChars(env, devNameId, 0)) == NULL) {
			_throwIllegalArgumentException(env, "Invalid parameter device name: cannot be null");
			break;
		}
		DEBUG(" < DD > GNU IO Native lite serial driver: open device %s\n", devName);
		if (optionsId) {
			key = options = strdup((*env)->GetStringUTFChars(env, optionsId, 0));
			DEBUG(" < DD > GNU IO Native lite serial driver: options: %s\n", options);
			do {
				if (key[0] == '\0') {
					break;
				}
				while(isspace(*key)) key++;
				val = strchr(key, '=');
				if (val[0] == '\0') {
					break;
				}
				val[0] = 0;
				val++;
				while(isspace(*val)) val++;
				next = strchr(val, ';');
				if (next) {
					next[0] = 0;
					next++;
				}
				DEBUG(" < DD > GNU IO Native lite serial driver: option %s = %s\n", key, val);
				if (strcmp(key, "baudrate") == 0) {
					baudrate = atoi(val);
				} else if (strncmp(key, "bitsperchar", 11) == 0) {
					bitsperchar = atoi(val);
				} else if (strncmp(key, "stopbits", 8) == 0) {
					stopbits = atoi(val);
				} else if (strncmp(key, "parity", 6) == 0) {
					parity = val;
				} else if (strncmp(key, "blocking", 8) == 0) {
					blocking = (strncmp(val, "on", 2) == 0? 1:0);
				} else if (strncmp(key, "autocts", 7) == 0) {
					autocts = (strncmp(val, "on", 2) == 0? 1:0);
				} else if (strncmp(key, "autorts", 7) == 0) {
					autorts = (strncmp(val, "on", 2) == 0? 1:0);
				} else {
					DEBUG(" < DD > GNU IO Native lite serial driver: Unsupported option: %s\n", key);
				}
			} while ((key = next) != NULL);
		}
		if (error != NULL) {
			break;
		}
		BREAK_ON_FAIL(fd = open(devName ,O_RDWR | O_NOCTTY | O_NDELAY));
		BREAK_ON_FAIL(fcntl(fd, F_SETFL, 0));
		BREAK_ON_FAIL(tcgetattr(fd, &config));
		cfmakeraw(&config);
		
		config.c_cflag |= (CLOCAL | CREAD);
		
		DEBUG(" < DD > GNU IO Native lite serial driver: set baudrate: %d\n", baudrate);
		switch (baudrate) {
			case 1200:
				ret =  cfsetispeed(&config,B1200);
				ret += cfsetospeed(&config,B1200);
				break;
			case 2400:
				ret =  cfsetispeed(&config,B2400);
				ret += cfsetospeed(&config,B2400);
				break;
			case 4800:
				ret =  cfsetispeed(&config,B4800);
				ret += cfsetospeed(&config,B4800);
				break;
			case 9600:
				ret =  cfsetispeed(&config,B9600);
				ret += cfsetospeed(&config,B9600);
				break;
			case 19200:
				ret =  cfsetispeed(&config,B19200);
				ret += cfsetospeed(&config,B19200);
				break;
			case 38400:
				ret =  cfsetispeed(&config,B38400);
				ret += cfsetospeed(&config,B38400);
				break;
			case 57600:
				ret =  cfsetispeed(&config,B57600);
				ret += cfsetospeed(&config,B57600);
				break;
			case 115200:
				ret =  cfsetispeed(&config,B115200);
				ret += cfsetospeed(&config,B115200);
				break;
			case 1500000:
				ret =  cfsetispeed(&config,B1500000);
				ret += cfsetospeed(&config,B1500000);
				break;
			case 2000000:
				ret =  cfsetispeed(&config,B2000000);
				ret += cfsetospeed(&config,B2000000);
				break;
			case 2500000:
				ret =  cfsetispeed(&config,B2500000);
				ret += cfsetospeed(&config,B2500000);
				break;
			case 3000000:
				ret =  cfsetispeed(&config,B3000000);
				ret += cfsetospeed(&config,B3000000);
				break;
			case 3500000:
				ret =  cfsetispeed(&config,B3500000);
				ret += cfsetospeed(&config,B3500000);
				break;
			case 4000000:
				ret =  cfsetispeed(&config,B4000000);
				ret += cfsetospeed(&config,B4000000);
				break;
			default:
				ret = -1;
				error = "Unsupported baud rate";
		}
		if (ret != 0) {
			break;
		}
		
		DEBUG(" < DD > GNU IO Native lite serial driver: set bitsperchar: %d\n", bitsperchar);
		switch (bitsperchar) {
			case 7:
				config.c_cflag |= CS7;
				break;
			case 8:
				config.c_cflag |= CS8;
				break;
			default:
				error = "Unsupported data size";
				ret = -1;
		}
		if (ret != 0) {
			break;
		}
		
		DEBUG(" < DD > GNU IO Native lite serial driver: set parity: %s\n", parity);
		switch (parity[0]) {
			case 'n':
			case 'N':
				config.c_cflag &= ~PARENB; // Clear parity enable
				break;
			case 'o':
			case 'O':
				config.c_cflag |= PARENB; // Parity enable
				config.c_cflag |= PARODD; // Enable odd parity 
				break;
			case 'e':
			case 'E':
				config.c_cflag |= PARENB; // Parity enable
				config.c_cflag &= ~PARODD; // Turn off odd parity = even
				break;
			default:
				error = "Unsupported parity\n";
				ret = -1;
		}
		if (ret != 0) {
			break;
		}
		
		DEBUG(" < DD > GNU IO Native lite serial driver: set stopbits: %d\n", stopbits);
		switch (stopbits) {
			case 1:
				config.c_cflag &= ~ CSTOPB;
				break;
			case 2:
				config.c_cflag |= CSTOPB;
				break;
			default:
				error = "Unsupported stop bits\n";
				ret = -1;
		}
		if (ret != 0) {
			break;
		}
		
		if (blocking == 0) {
			fprintf(stderr, " < WW > GNU IO Native lite serial driver: blocking=off not supported yet\n");
		}
		
		config.c_cc[VMIN] = 1;
		config.c_cc[VTIME] = 0;

		// Control flags
		if (autorts && autocts) {
			DEBUG(" < DD > GNU IO Native lite serial driver: RTS/CTS enabled\n");
			config.c_cflag |= CRTSCTS; // Enable RTS/CTS
		} else {
			DEBUG(" < DD > GNU IO Native lite serial driver: RTS/CTS disabled\n");
			config.c_cflag &= ~CRTSCTS; // Enable RTS/CTS
		}
		config.c_cflag |= CLOCAL;
		config.c_cflag |= CREAD;
		config.c_iflag &= ~(IXON | IXOFF | IXANY);
		
		BREAK_ON_FAIL(tcsetattr(fd, TCSANOW, &config));
		
		DEBUG(" < DD > GNU IO Native lite serial driver: open success\n");
		return fd;
	} while(0);
	if ((error != NULL && asprintf(&val, "Fail to open %s: %s", devName, error) != -1) 
		|| asprintf(&val, "Fail to open %s: %s", devName, strerror(errno)) != -1) {
		DEBUG(" < DD > GNU IO Native lite serial driver: open failure with error: %s\n", val);
		_throwIOException(env, val);
		free(val);
	}
	if (fd != -1) {
		close(fd);
	}
	free(options);
	return fd;
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _close
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mbedsys_io_SerialDriver__1close
  (JNIEnv *env, jclass klass, jint fd) {
	DEBUG(" < DD > GNU IO Native lite serial driver: close\n");
	close(fd);
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _available
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mbedsys_io_SerialDriver__1available
  (JNIEnv *env, jclass klass, jint fd) {
	char *error_message;
	int result;
	if( ioctl( fd, FIONREAD, &result ) < 0 ) {
		if (asprintf(&error_message, "Fail to write: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: get available failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
	DEBUG(" < DD > GNU IO Native lite serial driver: available: %d\n", result);
	return result;
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _read
 * Signature: (I[BII)I
 */
JNIEXPORT jint JNICALL Java_org_mbedsys_io_SerialDriver__1read__I_3BII
  (JNIEnv *env, jclass klass, jint fd, jbyteArray b, jint off, jint len) {
	char *error_message;
	int ret = 0,readCnt = 0;
	jbyte* data = (*env)->GetByteArrayElements( env, b, 0 );
	do {
		ret = read(fd, (void * ) ((char *) data + readCnt + off), len - readCnt);
		if(ret > 0){
			readCnt += ret;
		}
	}  while ((readCnt < len ) && (ret > 0 || errno == EINTR));
	if (ret < 0) {
		if (asprintf(&error_message, "Fail to write: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: read byte array failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
	DEBUG(" < DD > GNU IO Native lite serial driver: read byte array:");
	DEBUG_ARRAY(data, off, len);
	DEBUG("\n");
	(*env)->ReleaseByteArrayElements(env, b, data, 0);
	return readCnt;
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _read
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mbedsys_io_SerialDriver__1read__I
  (JNIEnv *env, jclass klass, jint fd) {
	char *error_message;
	int ret;
	char data;
	ret = read(fd, &data, 1);
	if (ret < 0) {
		if (asprintf(&error_message, "Fail to write: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: read byte failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
	DEBUG(" < DD > GNU IO Native lite serial driver: read byte: %02X\n", (ret? data: -1) & 0xFF);
	return ret? (data & 0xFF): -1;
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _write
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mbedsys_io_SerialDriver__1write__II
  (JNIEnv *env, jclass klass, jint fd, jint b) {
	char *error_message;
	char data = (char)b;
	if (write(fd, &data, 1) < 0) {
		if (asprintf(&error_message, "Fail to write: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: write byte failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
	DEBUG(" < DD > GNU IO Native lite serial driver: write byte: %02X\n", b & 0xFF);
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _write
 * Signature: (I[BII)V
 */
JNIEXPORT void JNICALL Java_org_mbedsys_io_SerialDriver__1write__I_3BII
  (JNIEnv *env, jclass klass, jint fd, jbyteArray b, jint off, jint len) {
	char *error_message;
	int ret = 0,writeCnt = 0;
	jbyte* data = (*env)->GetByteArrayElements(env, b, 0);
	do {
		ret = write(fd, (void * ) ((char *) data + writeCnt + off), len - writeCnt);
		if(ret > 0){
			writeCnt += ret;
		}
	}  while ((writeCnt < len ) && (ret > 0 || errno == EINTR));
	if (ret < 0) {
		if (asprintf(&error_message, "Fail to write: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: write byte array failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
	DEBUG(" < DD > GNU IO Native lite serial driver: write byte array:");
	DEBUG_ARRAY(data, off, len);
	DEBUG("\n");
	(*env)->ReleaseByteArrayElements(env, b, data, 0);
}

/*
 * Class:     org_mbedsys_io_SerialDriver
 * Method:    _flush
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mbedsys_io_SerialDriver__1flush
  (JNIEnv *env, jclass klass, jint fd) {
	char *error_message;
	int ret = tcdrain(fd);
	if (ret < 0) {
		if (asprintf(&error_message, "Fail to flush: %s", strerror(errno)) != -1) {
			DEBUG(" < DD > GNU IO Native lite serial driver: flush failure with error: %s\n", error_message);
			_throwIOException(env, error_message);
			free(error_message);
		}
	}
}
