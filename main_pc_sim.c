#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#ifdef LPTR
#undef LPTR
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "pc_sim_runtime.h"
#include "APP_CTRL/APP_SYS/Src/APP_SYS.h"

#if defined(__has_include)
#if __has_include("2_DRV/CL42T/Src/CL42T.h")
#include "2_DRV/CL42T/Src/CL42T.h"
#define PCSIM_CL42T_HEADER_FOUND 1
#elif __has_include("CL42T/Src/CL42T.h")
#include "CL42T/Src/CL42T.h"
#define PCSIM_CL42T_HEADER_FOUND 1
#endif
#endif

#if !defined(PCSIM_CL42T_HEADER_FOUND)
t_eReturnCode CL42T_Init(void);
t_eReturnCode CL42T_Cyclic(void);
#endif

#define PCSIM_DEFAULT_UDP_PORT 19090
#define PCSIM_MAX_CMD_LEN 8192
#define PCSIM_MAX_UDP_CMDS_PER_STEP 32
#define PCSIM_CAN_BURST_MAX 64
#define PCSIM_CAN_RX_BURST_MAX 64

typedef struct
{
    t_uint16 udpPort_u16;
    t_uint16 sleepMs_u16;
} t_sPcSimOptions;

#if defined(_WIN32)
typedef SOCKET pcsim_socket_t;
#else
typedef int pcsim_socket_t;
#endif

static void s_printUsage(void)
{
    printf("PCSIM options:\n");
    printf("  --udp-port <port>   UDP command port (default 19090)\n");
    printf("  --sleep-ms <ms>     Main-loop sleep in ms (default 1)\n");
    printf("  --ana <id> <value>  Set startup analog value (repeatable)\n");
    printf("  --enc-map <enc_idx> <pwm_idx> <pulses_per_rev> [dir_dig_idx]  Bind encoder to PWM pulse source and optional direction pin (repeatable)\n");
    printf("  --help              Show this help\n");
}

static volatile sig_atomic_t g_pcSimKeepRunning_s32 = 1;

static void s_signalHandler(int signal_s32)
{
    (void)signal_s32;
    g_pcSimKeepRunning_s32 = 0;
}

static int s_applyOptions(int argc, char **argv, t_sPcSimOptions *opt_ps)
{
    int idx_s32;

    opt_ps->udpPort_u16 = PCSIM_DEFAULT_UDP_PORT;
    opt_ps->sleepMs_u16 = 1U;

    for (idx_s32 = 1; idx_s32 < argc; idx_s32++)
    {
        if (strcmp(argv[idx_s32], "--help") == 0)
        {
            s_printUsage();
            return 1;
        }
        if ((strcmp(argv[idx_s32], "--udp-port") == 0) && ((idx_s32 + 1) < argc))
        {
            opt_ps->udpPort_u16 = (t_uint16)atoi(argv[++idx_s32]);
            continue;
        }
        if (strcmp(argv[idx_s32], "--udp-port") == 0)
        {
            printf("PCSIM: missing value for --udp-port (using default %u)\n", (unsigned)opt_ps->udpPort_u16);
            continue;
        }
        if ((strcmp(argv[idx_s32], "--sleep-ms") == 0) && ((idx_s32 + 1) < argc))
        {
            opt_ps->sleepMs_u16 = (t_uint16)atoi(argv[++idx_s32]);
            continue;
        }
        if (strcmp(argv[idx_s32], "--sleep-ms") == 0)
        {
            printf("PCSIM: missing value for --sleep-ms (using default %u)\n", (unsigned)opt_ps->sleepMs_u16);
            continue;
        }
        if ((strcmp(argv[idx_s32], "--ana") == 0) && ((idx_s32 + 2) < argc))
        {
            t_eFMKIO_InAnaSig sig_e = (t_eFMKIO_InAnaSig)atoi(argv[++idx_s32]);
            t_float32 val_f32 = (t_float32)atof(argv[++idx_s32]);
            (void)PCSIM_SetAnalogInput(sig_e, val_f32);
            continue;
        }
        if (strcmp(argv[idx_s32], "--ana") == 0)
        {
            printf("PCSIM: missing value(s) for --ana, expected: --ana <id> <value>\n");
            continue;
        }
        if ((strcmp(argv[idx_s32], "--enc-map") == 0) && ((idx_s32 + 3) < argc))
        {
            t_eFMKIO_InEcdrSignals enc_e = (t_eFMKIO_InEcdrSignals)atoi(argv[++idx_s32]);
            t_eFMKIO_OutPwmSig pwm_e = (t_eFMKIO_OutPwmSig)atoi(argv[++idx_s32]);
            t_float32 ppr_f32 = (t_float32)atof(argv[++idx_s32]);
            t_eFMKIO_OutDigSig dirDig_e = (t_eFMKIO_OutDigSig)pwm_e;

            if (((idx_s32 + 1) < argc) && (argv[idx_s32 + 1][0] != '-'))
            {
                dirDig_e = (t_eFMKIO_OutDigSig)atoi(argv[++idx_s32]);
            }

            t_eReturnCode ret_e = PCSIM_RuntimeSetEncoderPulseMapping(enc_e, pwm_e, ppr_f32, dirDig_e);
            if (ret_e != RC_OK)
            {
                printf("PCSIM: invalid --enc-map values (enc=%u pwm=%u ppr=%.3f dir=%u)\n",
                       (unsigned)enc_e,
                       (unsigned)pwm_e,
                       (double)ppr_f32,
                       (unsigned)dirDig_e);
            }
            continue;
        }
        if (strcmp(argv[idx_s32], "--enc-map") == 0)
        {
            printf("PCSIM: missing value(s) for --enc-map, expected: --enc-map <enc_idx> <pwm_idx> <pulses_per_rev> [dir_dig_idx]\n");
            continue;
        }

        printf("PCSIM: ignoring unknown argument '%s'\n", argv[idx_s32]);
    }

    return 0;
}

static int s_socketInit(pcsim_socket_t *sock, t_uint16 udpPort_u16)
{
    struct sockaddr_in addr_s;

#if defined(_WIN32)
    WSADATA wsaData_s;
    u_long nonBlocking_u32 = 1UL;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData_s) != 0)
    {
        return -1;
    }
#endif

    *sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (*sock == INVALID_SOCKET)
#else
    if (*sock < 0)
#endif
    {
        return -1;
    }

#if defined(_WIN32)
    if (ioctlsocket(*sock, FIONBIO, &nonBlocking_u32) != 0)
#else
    if (fcntl(*sock, F_SETFL, O_NONBLOCK) != 0)
#endif
    {
        return -1;
    }

    memset(&addr_s, 0, sizeof(addr_s));
    addr_s.sin_family = AF_INET;
    addr_s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_s.sin_port = htons(udpPort_u16);

    if (bind(*sock, (struct sockaddr *)&addr_s, sizeof(addr_s)) != 0)
    {
        return -1;
    }

    return 0;
}

static void s_socketClose(pcsim_socket_t sock)
{
#if defined(_WIN32)
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
}

static void s_sendReply(pcsim_socket_t sock,
                        const struct sockaddr_in *clientAddr_ps,
                        const char *reply_str)
{
    sendto(sock,
           reply_str,
           (int)strlen(reply_str),
           0,
           (const struct sockaddr *)clientAddr_ps,
           (socklen_t)sizeof(*clientAddr_ps));
}

static int s_parseCanCmd(char *cmd_str, t_uint8 *data_au8, t_uint8 *dlc_pu8,
                         t_eFMKFDCAN_NodeList *node_pe, t_uint32 *id_pu32)
{
    char *token_str;
    char *endPtr_str;
    unsigned long tmp_ul;
    t_uint8 idx_u8;

    token_str = strtok(cmd_str, " \t\r\n");
    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    tmp_ul = strtoul(token_str, &endPtr_str, 0);
    if (*endPtr_str != '\0')
    {
        return -1;
    }
    *node_pe = (t_eFMKFDCAN_NodeList)tmp_ul;

    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    *id_pu32 = (t_uint32)strtoul(token_str, &endPtr_str, 0);
    if (*endPtr_str != '\0')
    {
        return -1;
    }

    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    tmp_ul = strtoul(token_str, &endPtr_str, 0);
    if ((*endPtr_str != '\0') || (tmp_ul > 8U))
    {
        return -1;
    }
    *dlc_pu8 = (t_uint8)tmp_ul;

    for (idx_u8 = 0U; idx_u8 < *dlc_pu8; idx_u8++)
    {
        token_str = strtok(NULL, " \t\r\n");
        if (token_str == NULL)
        {
            return -1;
        }
        tmp_ul = strtoul(token_str, &endPtr_str, 0);
        if ((*endPtr_str != '\0') || (tmp_ul > 255U))
        {
            return -1;
        }
        data_au8[idx_u8] = (t_uint8)tmp_ul;
    }

    return 0;
}

static int s_parseCanCmdEx(char *cmd_str, t_uint8 *data_au8, t_uint8 *dlc_pu8,
                           t_eFMKFDCAN_NodeList *node_pe, t_uint32 *id_pu32, t_bool *isExtended_pb)
{
    char *token_str;
    char *endPtr_str;
    unsigned long tmp_ul;
    t_uint8 idx_u8;

    token_str = strtok(cmd_str, " \t\r\n");
    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    tmp_ul = strtoul(token_str, &endPtr_str, 0);
    if (*endPtr_str != '\0')
    {
        return -1;
    }
    *node_pe = (t_eFMKFDCAN_NodeList)tmp_ul;

    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    *id_pu32 = (t_uint32)strtoul(token_str, &endPtr_str, 0);
    if (*endPtr_str != '\0')
    {
        return -1;
    }

    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    tmp_ul = strtoul(token_str, &endPtr_str, 0);
    if ((*endPtr_str != '\0') || (tmp_ul > 1U))
    {
        return -1;
    }
    *isExtended_pb = (tmp_ul != 0U) ? TRUE : FALSE;

    token_str = strtok(NULL, " \t\r\n");
    if (token_str == NULL)
    {
        return -1;
    }
    tmp_ul = strtoul(token_str, &endPtr_str, 0);
    if ((*endPtr_str != '\0') || (tmp_ul > 8U))
    {
        return -1;
    }
    *dlc_pu8 = (t_uint8)tmp_ul;

    for (idx_u8 = 0U; idx_u8 < *dlc_pu8; idx_u8++)
    {
        token_str = strtok(NULL, " \t\r\n");
        if (token_str == NULL)
        {
            return -1;
        }
        tmp_ul = strtoul(token_str, &endPtr_str, 0);
        if ((*endPtr_str != '\0') || (tmp_ul > 255U))
        {
            return -1;
        }
        data_au8[idx_u8] = (t_uint8)tmp_ul;
    }

    return 0;
}

static void s_processCommand(pcsim_socket_t sock,
                             const char *cmd_str,
                             const struct sockaddr_in *clientAddr_ps)
{
    char reply_ac[PCSIM_MAX_CMD_LEN];
    char localCmd_ac[PCSIM_MAX_CMD_LEN];
    unsigned id_u32;
    int intVal_s32;
    int f2_s32;
    float floatVal_f32;
    float f2_f32;
    t_eReturnCode ret_e;

    if (strlen(cmd_str) >= sizeof(localCmd_ac))
    {
        s_sendReply(sock, clientAddr_ps, "ERR command-too-long\n");
        return;
    }

    strcpy(localCmd_ac, cmd_str);

    if (strncmp(localCmd_ac, "PING", 4) == 0)
    {
        s_sendReply(sock, clientAddr_ps, "OK PONG\n");
    }
    else if (sscanf(localCmd_ac, "SET_ANA %u %f", &id_u32, &floatVal_f32) == 2)
    {
        ret_e = PCSIM_SetAnalogInput((t_eFMKIO_InAnaSig)id_u32, (t_float32)floatVal_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_ANA %u", &id_u32) == 1)
    {
        t_float32 value_f32 = 0.0f;
        ret_e = PCSIM_GetAnalogInput((t_eFMKIO_InAnaSig)id_u32, &value_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %.3f\n", ret_e, value_f32);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_PWM %u %d", &id_u32, &intVal_s32) == 2)
    {
        ret_e = PCSIM_SetPwmDuty((t_eFMKIO_OutPwmSig)id_u32, (t_uint16)intVal_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_PWM %u", &id_u32) == 1)
    {
        t_uint16 duty_u16 = 0U;
        ret_e = PCSIM_GetPwmDuty((t_eFMKIO_OutPwmSig)id_u32, &duty_u16);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %u\n", ret_e, duty_u16);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_PWM_FREQ %u %f", &id_u32, &floatVal_f32) == 2)
    {
        ret_e = PCSIM_SetPwmFrequency((t_eFMKIO_OutPwmSig)id_u32, (t_float32)floatVal_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_PWM_FREQ %u", &id_u32) == 1)
    {
        t_float32 value_f32 = 0.0f;
        ret_e = PCSIM_GetPwmFrequency((t_eFMKIO_OutPwmSig)id_u32, &value_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %.3f\n", ret_e, value_f32);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_PWM_PULSES %u %d", &id_u32, &intVal_s32) == 2)
    {
        ret_e = PCSIM_SetPwmPulses((t_eFMKIO_OutPwmSig)id_u32, (t_uint16)intVal_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_PWM_PULSES %u", &id_u32) == 1)
    {
        t_uint16 value_u16 = 0U;
        ret_e = PCSIM_GetPwmPulses((t_eFMKIO_OutPwmSig)id_u32, &value_u16);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %u\n", ret_e, value_u16);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_IN_DIG %u %d", &id_u32, &intVal_s32) == 2)
    {
        ret_e = PCSIM_SetInputDigital((t_eFMKIO_InDigSig)id_u32, (t_eFMKIO_DigValue)intVal_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_IN_DIG %u", &id_u32) == 1)
    {
        t_eFMKIO_DigValue value_e = FMKIO_DIG_VALUE_LOW;
        ret_e = PCSIM_GetInputDigital((t_eFMKIO_InDigSig)id_u32, &value_e);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %u\n", ret_e, (unsigned)value_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_OUT_DIG %u %d", &id_u32, &intVal_s32) == 2)
    {
        ret_e = FMKIO_Set_OutDigSigValue((t_eFMKIO_OutDigSig)id_u32, (t_eFMKIO_DigValue)intVal_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_OUT_DIG %u", &id_u32) == 1)
    {
        t_eFMKIO_DigValue value_e = FMKIO_DIG_VALUE_LOW;
        ret_e = FMKIO_Get_OutDigSigValue((t_eFMKIO_OutDigSig)id_u32, &value_e);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %u\n", ret_e, (unsigned)value_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_IN_FREQ %u %f", &id_u32, &floatVal_f32) == 2)
    {
        ret_e = PCSIM_SetInputFrequency((t_eFMKIO_InFreqSig)id_u32, (t_float32)floatVal_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_IN_FREQ %u", &id_u32) == 1)
    {
        t_float32 value_f32 = 0.0f;
        ret_e = PCSIM_GetInputFrequency((t_eFMKIO_InFreqSig)id_u32, &value_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %.3f\n", ret_e, value_f32);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_ENC_POS %u %f %f", &id_u32, &floatVal_f32, &f2_f32) == 3)
    {
        ret_e = PCSIM_SetEncoderPosition((t_eFMKIO_InEcdrSignals)id_u32, (t_float32)floatVal_f32, (t_float32)f2_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_ENC_POS %u", &id_u32) == 1)
    {
        t_float32 abs_f32 = 0.0f;
        t_float32 rel_f32 = 0.0f;
        ret_e = PCSIM_GetEncoderPosition((t_eFMKIO_InEcdrSignals)id_u32, &abs_f32, &rel_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d ABS %.3f REL %.3f\n", ret_e, abs_f32, rel_f32);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_ENC_SPEED %u %f", &id_u32, &floatVal_f32) == 2)
    {
        ret_e = PCSIM_SetEncoderSpeed((t_eFMKIO_InEcdrSignals)id_u32, (t_float32)floatVal_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_ENC_MAP %u %u %f %d", &id_u32, &intVal_s32, &floatVal_f32, &f2_s32) == 4)
    {
        ret_e = PCSIM_RuntimeSetEncoderPulseMapping((t_eFMKIO_InEcdrSignals)id_u32,
                                                    (t_eFMKIO_OutPwmSig)intVal_s32,
                                                    (t_float32)floatVal_f32,
                                                    (t_eFMKIO_OutDigSig)f2_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "SET_ENC_MAP %u %u %f", &id_u32, &intVal_s32, &floatVal_f32) == 3)
    {
        ret_e = PCSIM_RuntimeSetEncoderPulseMapping((t_eFMKIO_InEcdrSignals)id_u32,
                                                    (t_eFMKIO_OutPwmSig)intVal_s32,
                                                    (t_float32)floatVal_f32,
                                                    (t_eFMKIO_OutDigSig)intVal_s32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "GET_ENC_SPEED %u", &id_u32) == 1)
    {
        t_float32 speed_f32 = 0.0f;
        ret_e = PCSIM_GetEncoderSpeed((t_eFMKIO_InEcdrSignals)id_u32, &speed_f32);
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d VAL %.3f\n", ret_e, speed_f32);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "INJECT_CAN_EX", 13) == 0)
    {
        t_uint8 data_au8[8];
        t_uint8 dlc_u8;
        t_eFMKFDCAN_NodeList node_e;
        t_uint32 canId_u32;
        t_bool isExt_b;

        if (s_parseCanCmdEx(localCmd_ac, data_au8, &dlc_u8, &node_e, &canId_u32, &isExt_b) == 0)
        {
            ret_e = PCSIM_InjectCanFrame(node_e, canId_u32, isExt_b, data_au8, dlc_u8);
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
            s_sendReply(sock, clientAddr_ps, reply_ac);
        }
        else
        {
            s_sendReply(sock, clientAddr_ps, "ERR bad-INJECT_CAN_EX\n");
        }
    }
    else if (strncmp(localCmd_ac, "INJECT_CAN", 10) == 0)
    {
        t_uint8 data_au8[8];
        t_uint8 dlc_u8;
        t_eFMKFDCAN_NodeList node_e;
        t_uint32 canId_u32;

        if (s_parseCanCmd(localCmd_ac, data_au8, &dlc_u8, &node_e, &canId_u32) == 0)
        {
            ret_e = PCSIM_InjectCanFrame(node_e, canId_u32, TRUE, data_au8, dlc_u8);
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
            s_sendReply(sock, clientAddr_ps, reply_ac);
        }
        else
        {
            s_sendReply(sock, clientAddr_ps, "ERR bad-INJECT_CAN\n");
        }
    }
    else if (strncmp(localCmd_ac, "GET_ALL", 7) == 0)
    {
        const t_float32 *ana_pf32 = PCSIM_GetAnalogSnapshot();
        const t_uint16 *pwm_pu16 = PCSIM_GetPwmSnapshot();
        int offset_s32 = 0;
        t_uint8 idx_u8;

        offset_s32 += snprintf(reply_ac + offset_s32,
                               sizeof(reply_ac) - (size_t)offset_s32,
                               "OK TICK %u ANA",
                               PCSIM_GetTickMs());
        for (idx_u8 = 0U; idx_u8 < FMKIO_INPUT_SIGANA_NB; idx_u8++)
        {
            offset_s32 += snprintf(reply_ac + offset_s32,
                                   sizeof(reply_ac) - (size_t)offset_s32,
                                   " %.1f",
                                   ana_pf32[idx_u8]);
        }

        offset_s32 += snprintf(reply_ac + offset_s32,
                               sizeof(reply_ac) - (size_t)offset_s32,
                               " PWM");

        for (idx_u8 = 0U; idx_u8 < FMKIO_OUTPUT_SIGPWM_NB; idx_u8++)
        {
            offset_s32 += snprintf(reply_ac + offset_s32,
                                   sizeof(reply_ac) - (size_t)offset_s32,
                                   " %u",
                                   pwm_pu16[idx_u8]);
        }

        offset_s32 += snprintf(reply_ac + offset_s32,
                               sizeof(reply_ac) - (size_t)offset_s32,
                               "\n");
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "GET_CAN_TX_COUNT", 16) == 0)
    {
        snprintf(reply_ac, sizeof(reply_ac), "OK COUNT %u\n", (unsigned)PCSIM_GetCanTxCount());
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "GET_CAN_BROKER_TX_COUNT", 23) == 0)
    {
        snprintf(reply_ac, sizeof(reply_ac), "OK COUNT %u\n", (unsigned)PCSIM_GetCanBrokerTxCount());
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "GET_CAN_RX_REG_COUNT", 20) == 0)
    {
        snprintf(reply_ac, sizeof(reply_ac), "OK COUNT %u\n", (unsigned)PCSIM_GetCanRxRegCount());
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "DUMP_CAN_RX_REG_BURST %d", &intVal_s32) == 1)
    {
        t_sPCSIM_CanRxRegInfo info_s;
        t_uint16 idx_u16;
        t_uint8 pushed_u8 = 0U;
        int off_s32 = 0;
        int maxRegs_s32 = intVal_s32;

        if (maxRegs_s32 < 1)
        {
            maxRegs_s32 = 1;
        }
        if (maxRegs_s32 > PCSIM_CAN_RX_BURST_MAX)
        {
            maxRegs_s32 = PCSIM_CAN_RX_BURST_MAX;
        }

        off_s32 = snprintf(reply_ac, sizeof(reply_ac), "OK RC 0");
        for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX && pushed_u8 < (t_uint8)maxRegs_s32; idx_u16++)
        {
            ret_e = PCSIM_GetCanRxRegAt(idx_u16, &info_s);
            if (ret_e != RC_OK)
            {
                break;
            }
            off_s32 += snprintf(reply_ac + off_s32,
                                sizeof(reply_ac) - (size_t)off_s32,
                                " REG NODE %u ID %u MASK %u EXT %u",
                                (unsigned)info_s.node_e,
                                (unsigned)info_s.identifier_u32,
                                (unsigned)info_s.mask_u32,
                                (unsigned)(info_s.idType_e == FMKFDCAN_IDTYPE_EXTENDED));
            pushed_u8++;
            if (off_s32 >= (int)sizeof(reply_ac))
            {
                break;
            }
        }
        if (pushed_u8 == 0U)
        {
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", RC_WARNING_NO_OPERATION);
        }
        else
        {
            snprintf(reply_ac + off_s32,
                     sizeof(reply_ac) - (size_t)off_s32,
                     "\n");
        }
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "POP_CAN_TX_BURST %d", &intVal_s32) == 1)
    {
        t_sPCSIM_CanTxFrame frame_s;
        t_uint8 idx_u8;
        t_uint8 dlc_u8;
        int off_s32;
        int maxFrames_s32;
        int pushed_s32;

        maxFrames_s32 = intVal_s32;
        if (maxFrames_s32 < 1)
        {
            maxFrames_s32 = 1;
        }
        if (maxFrames_s32 > PCSIM_CAN_BURST_MAX)
        {
            maxFrames_s32 = PCSIM_CAN_BURST_MAX;
        }

        off_s32 = snprintf(reply_ac, sizeof(reply_ac), "OK RC 0");
        pushed_s32 = 0;

        while ((pushed_s32 < maxFrames_s32) && (off_s32 < (int)sizeof(reply_ac)))
        {
            ret_e = PCSIM_PopCanTxFrame(&frame_s);
            if (ret_e != RC_OK)
            {
                break;
            }

            dlc_u8 = (t_uint8)frame_s.dlc_e;
            if (dlc_u8 > 64U)
            {
                dlc_u8 = 64U;
            }

            off_s32 += snprintf(reply_ac + off_s32,
                                sizeof(reply_ac) - (size_t)off_s32,
                                " FRAME TS %u NODE %u ID %u EXT %u DLC %u DATA",
                                (unsigned)frame_s.timeStamp_u32,
                                (unsigned)frame_s.node_e,
                                (unsigned)frame_s.identifier_u32,
                                (unsigned)(frame_s.idType_e == FMKFDCAN_IDTYPE_EXTENDED),
                                (unsigned)dlc_u8);
            for (idx_u8 = 0U; (idx_u8 < dlc_u8) && (off_s32 < (int)sizeof(reply_ac)); idx_u8++)
            {
                off_s32 += snprintf(reply_ac + off_s32,
                                    sizeof(reply_ac) - (size_t)off_s32,
                                    " %u",
                                    (unsigned)frame_s.data_au8[idx_u8]);
            }
            pushed_s32++;
        }

        if (pushed_s32 == 0)
        {
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", RC_WARNING_NO_OPERATION);
        }
        else
        {
            snprintf(reply_ac + off_s32,
                     sizeof(reply_ac) - (size_t)off_s32,
                     "\n");
        }
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (sscanf(localCmd_ac, "POP_CAN_BROKER_TX_BURST %d", &intVal_s32) == 1)
    {
        t_sPCSIM_CanTxFrame frame_s;
        t_uint8 idx_u8;
        t_uint8 dlc_u8;
        int off_s32;
        int maxFrames_s32;
        int pushed_s32;

        maxFrames_s32 = intVal_s32;
        if (maxFrames_s32 < 1)
        {
            maxFrames_s32 = 1;
        }
        if (maxFrames_s32 > PCSIM_CAN_BURST_MAX)
        {
            maxFrames_s32 = PCSIM_CAN_BURST_MAX;
        }

        off_s32 = snprintf(reply_ac, sizeof(reply_ac), "OK RC 0");
        pushed_s32 = 0;

        while ((pushed_s32 < maxFrames_s32) && (off_s32 < (int)sizeof(reply_ac)))
        {
            ret_e = PCSIM_PopCanBrokerTxFrame(&frame_s);
            if (ret_e != RC_OK)
            {
                break;
            }

            dlc_u8 = (t_uint8)frame_s.dlc_e;
            if (dlc_u8 > 64U)
            {
                dlc_u8 = 64U;
            }

            off_s32 += snprintf(reply_ac + off_s32,
                                sizeof(reply_ac) - (size_t)off_s32,
                                " FRAME TS %u NODE %u ID %u EXT %u DLC %u DATA",
                                (unsigned)frame_s.timeStamp_u32,
                                (unsigned)frame_s.node_e,
                                (unsigned)frame_s.identifier_u32,
                                (unsigned)(frame_s.idType_e == FMKFDCAN_IDTYPE_EXTENDED),
                                (unsigned)dlc_u8);
            for (idx_u8 = 0U; (idx_u8 < dlc_u8) && (off_s32 < (int)sizeof(reply_ac)); idx_u8++)
            {
                off_s32 += snprintf(reply_ac + off_s32,
                                    sizeof(reply_ac) - (size_t)off_s32,
                                    " %u",
                                    (unsigned)frame_s.data_au8[idx_u8]);
            }
            pushed_s32++;
        }

        if (pushed_s32 == 0)
        {
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", RC_WARNING_NO_OPERATION);
        }
        else
        {
            snprintf(reply_ac + off_s32,
                     sizeof(reply_ac) - (size_t)off_s32,
                     "\n");
        }
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "POP_CAN_TX", 10) == 0)
    {
        t_sPCSIM_CanTxFrame frame_s;
        t_uint8 idx_u8;
        t_uint8 dlc_u8;
        int off_s32;

        ret_e = PCSIM_PopCanTxFrame(&frame_s);
        if (ret_e != RC_OK)
        {
            snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
            s_sendReply(sock, clientAddr_ps, reply_ac);
        }
        else
        {
            dlc_u8 = (t_uint8)frame_s.dlc_e;
            if (dlc_u8 > 64U)
            {
                dlc_u8 = 64U;
            }
            off_s32 = snprintf(reply_ac,
                               sizeof(reply_ac),
                               "OK RC 0 TS %u NODE %u ID %u EXT %u DLC %u DATA",
                               (unsigned)frame_s.timeStamp_u32,
                               (unsigned)frame_s.node_e,
                               (unsigned)frame_s.identifier_u32,
                               (unsigned)(frame_s.idType_e == FMKFDCAN_IDTYPE_EXTENDED),
                               (unsigned)dlc_u8);
            for (idx_u8 = 0U; (idx_u8 < dlc_u8) && (off_s32 < (int)sizeof(reply_ac)); idx_u8++)
            {
                off_s32 += snprintf(reply_ac + off_s32,
                                    sizeof(reply_ac) - (size_t)off_s32,
                                    " %u",
                                    (unsigned)frame_s.data_au8[idx_u8]);
            }
            snprintf(reply_ac + off_s32,
                     sizeof(reply_ac) - (size_t)off_s32,
                     "\n");
            s_sendReply(sock, clientAddr_ps, reply_ac);
        }
    }
    else if (strncmp(localCmd_ac, "CLEAR_CAN_TX", 12) == 0)
    {
        ret_e = PCSIM_ClearCanTxFrames();
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "CLEAR_CAN_BROKER_TX", 19) == 0)
    {
        ret_e = PCSIM_ClearCanBrokerTxFrames();
        snprintf(reply_ac, sizeof(reply_ac), "OK RC %d\n", ret_e);
        s_sendReply(sock, clientAddr_ps, reply_ac);
    }
    else if (strncmp(localCmd_ac, "HELP", 4) == 0)
    {
        s_sendReply(sock,
                    clientAddr_ps,
                    "OK CMDS: PING HELP GET_ALL SET_ANA/GET_ANA SET_PWM/GET_PWM SET_PWM_FREQ/GET_PWM_FREQ SET_PWM_PULSES/GET_PWM_PULSES SET_IN_DIG/GET_IN_DIG SET_OUT_DIG/GET_OUT_DIG SET_IN_FREQ/GET_IN_FREQ SET_ENC_POS/GET_ENC_POS SET_ENC_SPEED/GET_ENC_SPEED SET_ENC_MAP INJECT_CAN INJECT_CAN_EX GET_CAN_TX_COUNT GET_CAN_BROKER_TX_COUNT GET_CAN_RX_REG_COUNT DUMP_CAN_RX_REG_BURST POP_CAN_TX POP_CAN_TX_BURST POP_CAN_BROKER_TX_BURST CLEAR_CAN_TX CLEAR_CAN_BROKER_TX\n");
    }
    else
    {
        s_sendReply(sock, clientAddr_ps, "ERR unknown-command\n");
    }
}

static void s_serverStep(pcsim_socket_t sock)
{
    char cmd_ac[PCSIM_MAX_CMD_LEN];
    struct sockaddr_in clientAddr_s;
    socklen_t clientLen_s;
    int recvLen_s32;
    int processed_s32 = 0;

    while (processed_s32 < PCSIM_MAX_UDP_CMDS_PER_STEP)
    {
        clientLen_s = (socklen_t)sizeof(clientAddr_s);
        recvLen_s32 = recvfrom(sock,
                               cmd_ac,
                               (int)(sizeof(cmd_ac) - 1U),
                               0,
                               (struct sockaddr *)&clientAddr_s,
                               &clientLen_s);
#if defined(_WIN32)
        if (recvLen_s32 == SOCKET_ERROR)
        {
            int err_s32 = WSAGetLastError();
            if ((err_s32 == WSAEWOULDBLOCK) || (err_s32 == WSAETIMEDOUT))
            {
                break;
            }
            break;
        }
#else
        if (recvLen_s32 < 0)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                break;
            }
            break;
        }
#endif

        cmd_ac[recvLen_s32] = '\0';
        s_processCommand(sock, cmd_ac, &clientAddr_s);
        processed_s32++;
    }
}

int main(int argc, char **argv)
{
    pcsim_socket_t serverSock;
    t_sPcSimOptions opt_s;
    int parseRet_s32;//

    PCSIM_RuntimeInit();
    signal(SIGINT, s_signalHandler);
    signal(SIGTERM, s_signalHandler);
#if defined(SIGBREAK)
    signal(SIGBREAK, s_signalHandler);
#endif

    parseRet_s32 = s_applyOptions(argc, argv, &opt_s);
    if (parseRet_s32 != 0)
    {
        return (parseRet_s32 > 0) ? 0 : 1;
    }

    if (s_socketInit(&serverSock, opt_s.udpPort_u16) != 0)
    {
        printf("PCSIM: failed to start UDP server on 127.0.0.1:%u\n", opt_s.udpPort_u16);
        return 1;
    }

    printf("PCSIM: UDP command server ready on 127.0.0.1:%u\n", opt_s.udpPort_u16);
    fflush(stdout);

    //---- special gamma compiling ----//
    APPSYS_Init();
    CL42T_Init();

    while (g_pcSimKeepRunning_s32 != 0)
    {
        s_serverStep(serverSock);
        PCSIM_RuntimeStep();
        APPSYS_Cyclic();

        //---- special gamma compiling ----//
        CL42T_Cyclic();
        PCSIM_InternalSleepMs((t_uint32)opt_s.sleepMs_u16);
    }

    s_socketClose(serverSock);
    return 0;
}
