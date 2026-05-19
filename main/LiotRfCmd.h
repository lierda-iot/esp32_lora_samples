#ifndef _LIOT_RF_CMD_H_
#define _LIOT_RF_CMD_H_

#include <stddef.h>
#include <stdint.h>

typedef int (*PrintfCallback)(uint8_t *data, size_t length);

int LiotRfCmdInit(void);
int LiotRfCmdSetPrintfCallback(PrintfCallback callback);
int LiotRfCmdExe(int argc, const char *const *argv);
char **LiotRfCmdComplet(int argc, const char *const *argv);
void LiotRfCmdSigint(void);

#endif
