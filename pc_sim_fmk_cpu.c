#include "pc_sim_runtime.h"
#include <time.h>
#include "FMK_HAL/FMK_CPU/Src/FMK_CPU.h"

static t_sFMKCPU_CoreFaultInfo g_pcSimCoreFaultInfo_s;

static t_bool g_pcSimRtcInit_b = (t_bool)FALSE;
static t_bool g_pcSimRtcDateTimeValid_b = (t_bool)FALSE;
static time_t g_pcSimRtcBaseTime_s = (time_t)0;
static t_uint32 g_pcSimRtcBaseTick_u32 = (t_uint32)0;

/**
 * @brief Return TRUE when the supplied year is a Gregorian leap year.
 */
static t_bool s_PCSIM_FMKCPU_IsLeapYear(t_uint16 f_Year_u16)
{
    t_bool IsLeapYear_b = (t_bool)FALSE;

    if(((f_Year_u16 % 4U) == 0U) &&
       (((f_Year_u16 % 100U) != 0U) || ((f_Year_u16 % 400U) == 0U)))
    {
        IsLeapYear_b = (t_bool)TRUE;
    }

    return IsLeapYear_b;
}

/**
 * @brief Return the number of days in one calendar month.
 */
static t_uint8 s_PCSIM_FMKCPU_GetDaysInMonth(t_uint16 f_Year_u16,
                                             t_uint8 f_Month_u8)
{
    static const t_uint8 DaysPerMonth_au8[12U] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };
    t_uint8 Days_u8 = 0U;

    if((f_Month_u8 >= 1U) && (f_Month_u8 <= 12U))
    {
        Days_u8 = DaysPerMonth_au8[f_Month_u8 - 1U];

        if((f_Month_u8 == 2U) &&
           (s_PCSIM_FMKCPU_IsLeapYear(f_Year_u16) == (t_bool)TRUE))
        {
            Days_u8 = 29U;
        }
    }

    return Days_u8;
}

/**
 * @brief Validate one public RTC time.
 */
static t_eReturnCode s_PCSIM_FMKCPU_ValidateTime(
    const t_sFMKCPU_Time * f_Time_ps)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_Time_ps == (const t_sFMKCPU_Time *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if((f_Time_ps->hour_u8 > 23U) ||
            (f_Time_ps->minute_u8 > 59U) ||
            (f_Time_ps->second_u8 > 59U))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }

    return Ret_e;
}

/**
 * @brief Validate one public RTC date.
 */
static t_eReturnCode s_PCSIM_FMKCPU_ValidateDate(
    const t_sFMKCPU_Date * f_Date_ps)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint8 DaysInMonth_u8 = 0U;

    if(f_Date_ps == (const t_sFMKCPU_Date *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if((f_Date_ps->year_u16 < 2000U) ||
            (f_Date_ps->year_u16 > 2099U) ||
            (f_Date_ps->month_u8 < 1U) ||
            (f_Date_ps->month_u8 > 12U) ||
            (f_Date_ps->weekDay_u8 < 1U) ||
            (f_Date_ps->weekDay_u8 > 7U))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        DaysInMonth_u8 =
            s_PCSIM_FMKCPU_GetDaysInMonth(f_Date_ps->year_u16,
                                          f_Date_ps->month_u8);

        if((f_Date_ps->day_u8 < 1U) ||
           (f_Date_ps->day_u8 > DaysInMonth_u8))
        {
            Ret_e = RC_ERROR_PARAM_INVALID;
        }
    }

    return Ret_e;
}

/**
 * @brief Convert a public date/time to the host local time representation.
 */
static t_eReturnCode s_PCSIM_FMKCPU_DateTimeToTimeT(
    const t_sFMKCPU_DateTime * f_DateTime_ps,
    time_t * f_Time_ps)
{
    t_eReturnCode Ret_e;
    struct tm TimeInfo_s = {0};
    time_t ConvertedTime_s;

    if((f_DateTime_ps == (const t_sFMKCPU_DateTime *)NULL) ||
       (f_Time_ps == (time_t *)NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        Ret_e = s_PCSIM_FMKCPU_ValidateDate(&f_DateTime_ps->date_s);

        if(Ret_e == RC_OK)
        {
            Ret_e = s_PCSIM_FMKCPU_ValidateTime(&f_DateTime_ps->time_s);
        }

        if(Ret_e == RC_OK)
        {
            TimeInfo_s.tm_year =
                (int)f_DateTime_ps->date_s.year_u16 - 1900;
            TimeInfo_s.tm_mon =
                (int)f_DateTime_ps->date_s.month_u8 - 1;
            TimeInfo_s.tm_mday =
                (int)f_DateTime_ps->date_s.day_u8;
            TimeInfo_s.tm_hour =
                (int)f_DateTime_ps->time_s.hour_u8;
            TimeInfo_s.tm_min =
                (int)f_DateTime_ps->time_s.minute_u8;
            TimeInfo_s.tm_sec =
                (int)f_DateTime_ps->time_s.second_u8;
            TimeInfo_s.tm_isdst = -1;

            ConvertedTime_s = mktime(&TimeInfo_s);

            if(ConvertedTime_s == (time_t)-1)
            {
                Ret_e = RC_ERROR_WRONG_RESULT;
            }
            else
            {
                *f_Time_ps = ConvertedTime_s;
            }
        }
    }

    return Ret_e;
}

/**
 * @brief Read the current simulated RTC absolute time.
 */
static t_eReturnCode s_PCSIM_FMKCPU_GetCurrentRtcTime(time_t * f_Time_ps)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint32 CurrentTick_u32;
    t_uint32 ElapsedMs_u32;

    if(f_Time_ps == (time_t *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_pcSimRtcInit_b == (t_bool)FALSE)
    {
        Ret_e = RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    else
    {
        CurrentTick_u32 = PCSIM_GetTickMs();

        /*
         * Unsigned subtraction also handles the 32-bit millisecond tick wrap
         * as long as two observations are less than one complete wrap apart.
         */
        ElapsedMs_u32 = CurrentTick_u32 - g_pcSimRtcBaseTick_u32;

        *f_Time_ps = g_pcSimRtcBaseTime_s +
                     (time_t)(ElapsedMs_u32 / 1000U);
    }

    return Ret_e;
}

/**
 * @brief Convert one host local time value to the public RTC structure.
 */
static t_eReturnCode s_PCSIM_FMKCPU_TimeTToDateTime(
    time_t f_Time_s,
    t_sFMKCPU_DateTime * f_DateTime_ps)
{
    t_eReturnCode Ret_e = RC_OK;
    struct tm * TimeInfo_ps;

    if(f_DateTime_ps == (t_sFMKCPU_DateTime *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        TimeInfo_ps = localtime(&f_Time_s);

        if(TimeInfo_ps == (struct tm *)NULL)
        {
            Ret_e = RC_ERROR_WRONG_RESULT;
        }
        else
        {
            f_DateTime_ps->date_s.year_u16 =
                (t_uint16)(TimeInfo_ps->tm_year + 1900);
            f_DateTime_ps->date_s.month_u8 =
                (t_uint8)(TimeInfo_ps->tm_mon + 1);
            f_DateTime_ps->date_s.day_u8 =
                (t_uint8)TimeInfo_ps->tm_mday;

            /* HAL convention: Monday = 1 ... Sunday = 7. */
            f_DateTime_ps->date_s.weekDay_u8 =
                (TimeInfo_ps->tm_wday == 0) ?
                (t_uint8)7U :
                (t_uint8)TimeInfo_ps->tm_wday;

            f_DateTime_ps->time_s.hour_u8 =
                (t_uint8)TimeInfo_ps->tm_hour;
            f_DateTime_ps->time_s.minute_u8 =
                (t_uint8)TimeInfo_ps->tm_min;
            f_DateTime_ps->time_s.second_u8 =
                (t_uint8)TimeInfo_ps->tm_sec;
        }
    }

    return Ret_e;
}

/**
 * @brief Set the simulated RTC absolute value and restart its elapsed-time base.
 */
static void s_PCSIM_FMKCPU_SetRtcBaseTime(time_t f_Time_s)
{
    g_pcSimRtcBaseTime_s = f_Time_s;
    g_pcSimRtcBaseTick_u32 = PCSIM_GetTickMs();
}


t_eReturnCode FMKCPU_Init(void)
{
    time_t HostTime_s;

    HostTime_s = time((time_t *)NULL);

    if(HostTime_s == (time_t)-1)
    {
        g_pcSimRtcInit_b = (t_bool)FALSE;
        g_pcSimRtcDateTimeValid_b = (t_bool)FALSE;
        return RC_ERROR_WRONG_RESULT;
    }

    /*
     * In PC simulation the host operating-system clock acts as the RTC
     * power/backup source, therefore it is considered valid at startup.
     */
    s_PCSIM_FMKCPU_SetRtcBaseTime(HostTime_s);
    g_pcSimRtcInit_b = (t_bool)TRUE;
    g_pcSimRtcDateTimeValid_b = (t_bool)TRUE;

    return RC_OK;
}
t_eReturnCode FMKCPU_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKCPU_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimCpuState_e;
    return RC_OK;
}

/*********************************
 * FMKCPU_GetLastCoreFaultInfo
 *********************************/
t_eReturnCode FMKCPU_GetLastCoreFaultInfo(t_sFMKCPU_CoreFaultInfo * f_faultInfo_ps)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- Check the destination used for the simulated fault snapshot ----//
    if(f_faultInfo_ps == (t_sFMKCPU_CoreFaultInfo *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        //---- Return the latest simulated core fault snapshot ----//
        *f_faultInfo_ps = g_pcSimCoreFaultInfo_s;
    }

    return Ret_e;
}

t_eReturnCode FMKCPU_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimCpuState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_SysClockCfg(t_eFMKCPU_CoreClockSpeed f_SystemCoreFreq_e)
{
    (void)f_SystemCoreFreq_e;
    return RC_OK;
}

void FMKCPU_Set_Delay(t_uint32 f_delayms_u32)
{
    PCSIM_InternalSleepMs(f_delayms_u32);
}

void FMKCPU_GetTick(t_uint32 *f_tickms_pu32)
{
    if (f_tickms_pu32 != NULL)
    {
        *f_tickms_pu32 = PCSIM_GetTickMs();
    }
}


/*********************************
 * FMKCPU_GetDateTime
 *********************************/
t_eReturnCode FMKCPU_GetDateTime(t_sFMKCPU_DateTime * f_DateTime_ps)
{
    t_eReturnCode Ret_e;
    time_t CurrentTime_s;

    if(f_DateTime_ps == (t_sFMKCPU_DateTime *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        Ret_e = s_PCSIM_FMKCPU_GetCurrentRtcTime(&CurrentTime_s);

        if(Ret_e == RC_OK)
        {
            Ret_e =
                s_PCSIM_FMKCPU_TimeTToDateTime(CurrentTime_s,
                                               f_DateTime_ps);
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_SetDateTime
 *********************************/
t_eReturnCode FMKCPU_SetDateTime(
    const t_sFMKCPU_DateTime * f_DateTime_ps)
{
    t_eReturnCode Ret_e;
    time_t NewTime_s;

    if(f_DateTime_ps == (const t_sFMKCPU_DateTime *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_pcSimRtcInit_b == (t_bool)FALSE)
    {
        Ret_e = RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    else
    {
        Ret_e =
            s_PCSIM_FMKCPU_DateTimeToTimeT(f_DateTime_ps, &NewTime_s);

        if(Ret_e == RC_OK)
        {
            s_PCSIM_FMKCPU_SetRtcBaseTime(NewTime_s);
            g_pcSimRtcDateTimeValid_b = (t_bool)TRUE;
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_GetDate
 *********************************/
t_eReturnCode FMKCPU_GetDate(t_sFMKCPU_Date * f_Date_ps)
{
    t_eReturnCode Ret_e;
    t_sFMKCPU_DateTime DateTime_s;

    if(f_Date_ps == (t_sFMKCPU_Date *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        Ret_e = FMKCPU_GetDateTime(&DateTime_s);

        if(Ret_e == RC_OK)
        {
            *f_Date_ps = DateTime_s.date_s;
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_SetDate
 *********************************/
t_eReturnCode FMKCPU_SetDate(const t_sFMKCPU_Date * f_Date_ps)
{
    t_eReturnCode Ret_e;
    t_sFMKCPU_DateTime DateTime_s;

    if(f_Date_ps == (const t_sFMKCPU_Date *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_pcSimRtcInit_b == (t_bool)FALSE)
    {
        Ret_e = RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    else
    {
        Ret_e = s_PCSIM_FMKCPU_ValidateDate(f_Date_ps);

        if(Ret_e == RC_OK)
        {
            Ret_e = FMKCPU_GetDateTime(&DateTime_s);
        }

        if(Ret_e == RC_OK)
        {
            DateTime_s.date_s = *f_Date_ps;

            /*
             * Reuse the complete setter so date normalization and simulated
             * RTC elapsed-time anchoring remain centralized.
             */
            Ret_e =
                s_PCSIM_FMKCPU_DateTimeToTimeT(&DateTime_s,
                                               &g_pcSimRtcBaseTime_s);

            if(Ret_e == RC_OK)
            {
                g_pcSimRtcBaseTick_u32 = PCSIM_GetTickMs();
            }
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_GetTime
 *********************************/
t_eReturnCode FMKCPU_GetTime(t_sFMKCPU_Time * f_Time_ps)
{
    t_eReturnCode Ret_e;
    t_sFMKCPU_DateTime DateTime_s;

    if(f_Time_ps == (t_sFMKCPU_Time *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        Ret_e = FMKCPU_GetDateTime(&DateTime_s);

        if(Ret_e == RC_OK)
        {
            *f_Time_ps = DateTime_s.time_s;
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_SetTime
 *********************************/
t_eReturnCode FMKCPU_SetTime(const t_sFMKCPU_Time * f_Time_ps)
{
    t_eReturnCode Ret_e;
    t_sFMKCPU_DateTime DateTime_s;
    time_t NewTime_s;

    if(f_Time_ps == (const t_sFMKCPU_Time *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_pcSimRtcInit_b == (t_bool)FALSE)
    {
        Ret_e = RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    else
    {
        Ret_e = s_PCSIM_FMKCPU_ValidateTime(f_Time_ps);

        if(Ret_e == RC_OK)
        {
            Ret_e = FMKCPU_GetDateTime(&DateTime_s);
        }

        if(Ret_e == RC_OK)
        {
            DateTime_s.time_s = *f_Time_ps;

            Ret_e =
                s_PCSIM_FMKCPU_DateTimeToTimeT(&DateTime_s,
                                               &NewTime_s);

            if(Ret_e == RC_OK)
            {
                s_PCSIM_FMKCPU_SetRtcBaseTime(NewTime_s);
            }
        }
    }

    return Ret_e;
}

/*********************************
 * FMKCPU_IsDateTimeValid
 *********************************/
t_eReturnCode FMKCPU_IsDateTimeValid(t_bool * f_IsValid_pb)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_IsValid_pb == (t_bool *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_pcSimRtcInit_b == (t_bool)FALSE)
    {
        Ret_e = RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    else
    {
        *f_IsValid_pb = g_pcSimRtcDateTimeValid_b;
    }

    return Ret_e;
}


t_eReturnCode FMKCPU_Set_HardwareInit(void) { return RC_OK; }

t_eReturnCode FMKCPU_Set_NVICState(t_eFMKCPU_IRQNType f_IRQN_e, t_eFMKCPU_NVIC_Ope f_OpeState_e)
{
    (void)f_IRQN_e;
    (void)f_OpeState_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_HwClock(t_eFMKCPU_ClockPort f_clkPort_e, t_eFMKCPU_ClockPortOpe f_OpeState_e)
{
    (void)f_clkPort_e;
    (void)f_OpeState_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_WwdgCfg(t_eFMKCPu_WwdgResetPeriod f_period_e)
{
    (void)f_period_e;
    return RC_OK;
}

void FMKCPU_RearmWwdg(void) {}

t_eReturnCode FMKCPU_RqstDmaInit(t_eFMKCPU_DmaRqst f_DmaRqstType,
                                 t_eFMKCPU_DmaType f_dmaType_e,
                                 void *f_ModuleHandle_pv)
{
    (void)f_DmaRqstType;
    (void)f_dmaType_e;
    (void)f_ModuleHandle_pv;
    return RC_OK;
}

t_eReturnCode FMKCPU_GetOscRccSrc(t_eFMKCPU_ClockPort f_clockPort_e,
                                  t_eFMKCPU_SysClkOsc *f_ClkOsc_pe)
{
    (void)f_clockPort_e;
    if (f_ClkOsc_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_ClkOsc_pe = (t_eFMKCPU_SysClkOsc)0;
    return RC_OK;
}

/*********************************
 * FMKCPU_GetRccClockValue
 *********************************/
t_eReturnCode FMKCPU_GetRccClockValue(t_eFMKCPU_ClockPort f_clockPort_e,
                                      t_uint16 * f_OscValueMHz_pu16)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- Check the requested RCC clock port ----//
    if(f_clockPort_e >= FMKCPU_RCC_CLK_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_OscValueMHz_pu16 == (t_uint16 *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        //---- Resolve the simulated oscillator connected to the port ----//
        t_eFMKCPU_SysClkOsc clkOsc_e = FMKCPU_SYS_CLOCK_NB;

        Ret_e = FMKCPU_GetOscRccSrc(f_clockPort_e, &clkOsc_e);

        if(Ret_e == RC_OK)
        {
            //---- Read the simulated oscillator frequency ----//
            Ret_e = FMKCPU_GetSysClkValue(clkOsc_e,
                                          f_OscValueMHz_pu16);
        }
    }

    return Ret_e;
}

t_eReturnCode FMKCPU_GetSysClkValue(t_eFMKCPU_SysClkOsc f_ClkOsc_e,
                                    t_uint16 *f_OscValueMHz_pu16)
{
    (void)f_ClkOsc_e;
    if (f_OscValueMHz_pu16 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_OscValueMHz_pu16 = 128U;
    return RC_OK;
}

DMA_HandleTypeDef *FMKCPU_PRIVATE_GetHandleTypeDef(t_eFMKCPU_DmaController f_dmaCtrl_e,
                                                    t_eFMKCPU_DmaChnl f_chnle_e)
{
    (void)f_dmaCtrl_e;
    (void)f_chnle_e;
    return (DMA_HandleTypeDef *)NULL;
}