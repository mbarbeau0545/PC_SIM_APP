/*********************************************************************
 * @file        pc_sim_fmknvm_backend.c
 * @brief       Persistent PC simulator backend for every FMK_NVM profile.
 *********************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "Library/SafeMem/SafeMem.h"
#include "FMK_CFG/FMKCFG_ConfigFiles/FMKNVM_ConfigPrivate.h"
#include "FMK_CFG/FMKCFG_ConfigSpecific/FMKNVM_ConfigSpecific.h"

/// @brief Maximum simulated storage capacity in bytes.
#define PCSIM_FMKNVM_STORAGE_MAX_SIZE ((t_uint32)1048576U)
/// @brief STM32H753 simulated erase-sector size.
#define PCSIM_FMKNVM_H753_ERASE_SIZE ((t_uint32)131072U)
/// @brief STM32H753 simulated Flash-word size.
#define PCSIM_FMKNVM_H753_PROGRAM_SIZE ((t_uint32)32U)
/// @brief STM32G4 simulated Flash-page size.
#define PCSIM_FMKNVM_G4_ERASE_SIZE ((t_uint32)2048U)
/// @brief STM32G4 simulated double-word size.
#define PCSIM_FMKNVM_G4_PROGRAM_SIZE ((t_uint32)8U)
/// @brief Simulated I2C EEPROM page size.
#define PCSIM_FMKNVM_I2C_PAGE_SIZE ((t_uint32)64U)
/// @brief Simulated SPI EEPROM page size.
#define PCSIM_FMKNVM_SPI_PAGE_SIZE ((t_uint32)128U)

/// @brief Runtime state of the file-backed simulated memory component.
typedef struct __t_sPCSIM_FMKNVM_Context
{
    const char * filePath_pc;         ///< Persistent image path.
    t_uint32 storageSize_u32;          ///< Accessible storage bytes.
    t_uint32 eraseUnitSize_u32;        ///< Erase granularity in bytes.
    t_uint32 programUnitSize_u32;      ///< Program granularity in bytes.
    t_uint32 commitUnitSize_u32;       ///< Commit marker granularity.
    t_uint32 pageSize_u32;             ///< EEPROM write-page size.
    t_bool eraseRequired_b;            ///< TRUE for simulated Flash.
    t_bool isInitialized_b;            ///< Backend initialization state.
    t_eReturnCode lastResult_e;         ///< Latest synchronous result.
} t_sPCSIM_FMKNVM_Context;

/// @brief File-backed simulated NVM bytes.
static t_uint8 g_PCSIM_FMKNVM_Storage_au8[
    PCSIM_FMKNVM_STORAGE_MAX_SIZE];

/// @brief Runtime context of the selected simulated memory profile.
static t_sPCSIM_FMKNVM_Context g_PCSIM_FMKNVM_Context_s;

/**
 * @brief Initialize the selected simulated storage profile and image file.
 * @param[in] f_Context_pv : Opaque PC simulator backend context.
 * @retval RC_OK The persistent image is ready.
 * @retval RC_ERROR_PTR_NULL The context pointer is null.
 * @retval RC_NVM_BACKEND_READ_ERROR The image could not be loaded.
 */
static t_eReturnCode s_PCSIM_FMKNVM_Init(void * f_Context_pv);
/**
 * @brief Return geometry matching the selected Flash or EEPROM profile.
 * @param[in] f_Context_pv : Initialized simulator backend context.
 * @param[out] f_Geometry_ps : Destination generic geometry.
 * @retval RC_OK Geometry was returned.
 * @retval RC_ERROR_PTR_NULL A supplied pointer is null.
 * @retval RC_ERROR_INSTANCE_NOT_INITIALIZED The backend is not initialized.
 */
static t_eReturnCode s_PCSIM_FMKNVM_GetGeometry(   void * f_Context_pv,
                                                   t_sFMKNVM_BackendGeometry * f_Geometry_ps);
/**
 * @brief Return capabilities matching the selected simulated technology.
 * @param[in] f_Context_pv : Initialized simulator backend context.
 * @param[out] f_Capabilities_ps : Destination generic capabilities.
 * @retval RC_OK Capabilities were returned.
 * @retval RC_ERROR_PTR_NULL A supplied pointer is null.
 * @retval RC_ERROR_INSTANCE_NOT_INITIALIZED The backend is not initialized.
 */
static t_eReturnCode s_PCSIM_FMKNVM_GetCapabilities(   void * f_Context_pv,
                                                       t_sFMKNVM_BackendCapabilities * f_Capabilities_ps);
/**
 * @brief Read bytes from the simulated persistent component.
 * @param[in] f_Context_pv : Initialized simulator backend context.
 * @param[in] f_Address_u32 : Backend-relative source offset.
 * @param[out] f_Data_pu8 : Caller-owned destination buffer.
 * @param[in] f_Size_u32 : Number of bytes to read.
 * @retval RC_OK Bytes were copied.
 * @retval RC_ERROR_PTR_NULL A supplied pointer is null.
 * @retval RC_ERROR_PARAM_INVALID The requested range is invalid.
 */
static t_eReturnCode s_PCSIM_FMKNVM_Read(   void * f_Context_pv,
                                            t_uint32 f_Address_u32,
                                            t_uint8 * f_Data_pu8,
                                            t_uint32 f_Size_u32);
/**
 * @brief Program bytes using the selected simulated technology semantics.
 * @note Flash profiles only permit one-to-zero transitions. I2C and SPI
 *       profiles split writes at their respective page boundaries and permit
 *       byte rewriting, matching EEPROM behavior without exposing a bus to
 *       FMK_NVM.
 * @param[in] f_Context_pv : Initialized simulator backend context.
 * @param[in] f_Address_u32 : Backend-relative destination offset.
 * @param[in] f_Data_pcu8 : Caller-owned source bytes.
 * @param[in] f_Size_u32 : Number of bytes to program.
 * @retval RC_OK Bytes and their persistent file were updated.
 * @retval RC_ERROR_PTR_NULL A supplied pointer is null.
 * @retval RC_ERROR_PARAM_INVALID Range or Flash alignment is invalid.
 * @retval RC_NVM_BACKEND_PROGRAM_ERROR Programming or persistence failed.
 */
static t_eReturnCode s_PCSIM_FMKNVM_Program(   void * f_Context_pv,
                                               t_uint32 f_Address_u32,
                                               const t_uint8 * f_Data_pcu8,
                                               t_uint32 f_Size_u32);
/**
 * @brief Erase an aligned range of the simulated persistent component.
 * @param[in] f_Context_pv : Initialized simulator backend context.
 * @param[in] f_Address_u32 : Backend-relative erase offset.
 * @param[in] f_Size_u32 : Aligned erase size.
 * @retval RC_OK The range contains erased bytes and was persisted.
 * @retval RC_ERROR_PTR_NULL The context pointer is null.
 * @retval RC_ERROR_PARAM_INVALID Range or alignment is invalid.
 * @retval RC_NVM_BACKEND_ERASE_ERROR Persistence failed.
 */
static t_eReturnCode s_PCSIM_FMKNVM_Erase(   void * f_Context_pv,
                                             t_uint32 f_Address_u32,
                                             t_uint32 f_Size_u32);
/**
 * @brief Report the synchronous simulator busy state.
 * @param[in] f_Context_pv : Simulator backend context.
 * @retval FALSE The current simulator performs bounded synchronous accesses.
 */
static t_bool s_PCSIM_FMKNVM_IsBusy(void * f_Context_pv);
/**
 * @brief Return the latest completed simulator operation result.
 * @param[in] f_Context_pv : Simulator backend context.
 * @param[out] f_OperationResult_pe : Destination operation result.
 * @retval RC_OK The result was returned.
 * @retval RC_ERROR_PTR_NULL A supplied pointer is null.
 */
static t_eReturnCode s_PCSIM_FMKNVM_GetOperationResult(   void * f_Context_pv,
                                                          t_eReturnCode * f_OperationResult_pe);
/**
 * @brief Service the synchronous simulated backend.
 * @param[in] f_Context_pv : Simulator backend context.
 */
static void s_PCSIM_FMKNVM_Cyclic(void * f_Context_pv);
/**
 * @brief Configure geometry and the default image path from EEPROM type.
 * @param[in,out] f_Context_ps : Context receiving its selected profile.
 * @retval RC_OK The selected profile is supported by PCSIM.
 * @retval RC_ERROR_NOT_SUPPORTED The EEPROM type is invalid.
 */
static t_eReturnCode s_PCSIM_FMKNVM_ConfigureProfile(   t_sPCSIM_FMKNVM_Context * f_Context_ps);
/**
 * @brief Load the persistent image or create an erased image when absent.
 * @param[in,out] f_Context_ps : Configured simulator context.
 * @retval RC_OK The RAM mirror and image file are synchronized.
 * @retval RC_NVM_BACKEND_READ_ERROR File access failed.
 */
static t_eReturnCode s_PCSIM_FMKNVM_LoadImage(   t_sPCSIM_FMKNVM_Context * f_Context_ps);
/**
 * @brief Persist a modified byte range to the simulator image.
 * @param[in] f_Context_ps : Configured simulator context.
 * @param[in] f_Address_u32 : First modified byte.
 * @param[in] f_Size_u32 : Number of modified bytes.
 * @retval RC_OK The range was flushed.
 * @retval RC_NVM_BACKEND_PROGRAM_ERROR File access failed.
 */
static t_eReturnCode s_PCSIM_FMKNVM_PersistRange(   const t_sPCSIM_FMKNVM_Context * f_Context_ps,
                                                    t_uint32 f_Address_u32,
                                                    t_uint32 f_Size_u32);
/**
 * @brief Validate one backend-relative byte range.
 * @param[in] f_Context_ps : Configured simulator context.
 * @param[in] f_Address_u32 : First requested byte.
 * @param[in] f_Size_u32 : Requested byte count.
 * @retval TRUE The complete range belongs to the component.
 * @retval FALSE The range is empty, overflows or exceeds the component.
 */
static t_bool s_PCSIM_FMKNVM_IsRangeValid(   const t_sPCSIM_FMKNVM_Context * f_Context_ps,
                                             t_uint32 f_Address_u32,
                                             t_uint32 f_Size_u32);

/// @brief Stable backend contract injected into the unchanged FMK_NVM core.
static const t_sFMKNVM_BackendApi c_PCSIM_FMKNVM_BackendApi_s =
{
    .context_pv = &g_PCSIM_FMKNVM_Context_s,
    .Init_pcb = s_PCSIM_FMKNVM_Init,
    .GetGeometry_pcb = s_PCSIM_FMKNVM_GetGeometry,
    .GetCapabilities_pcb = s_PCSIM_FMKNVM_GetCapabilities,
    .Read_pcb = s_PCSIM_FMKNVM_Read,
    .Program_pcb = s_PCSIM_FMKNVM_Program,
    .Erase_pcb = s_PCSIM_FMKNVM_Erase,
    .IsBusy_pcb = s_PCSIM_FMKNVM_IsBusy,
    .GetOperationResult_pcb = s_PCSIM_FMKNVM_GetOperationResult,
    .Cyclic_pcb = s_PCSIM_FMKNVM_Cyclic
};

/// @brief H753 simulation mapping with two 128-KiB slots per partition.
static const t_sFMKNVM_PartitionStorageCfg
    c_PCSIM_FMKNVM_H753Storage_as[FMKNVM_PARTITION_NB] =
{
    {0U, 262144U, 2U},
    {262144U, 262144U, 2U},
    {524288U, 262144U, 2U}
};

/// @brief G4 simulation mapping with two 4-KiB slots per partition.
static const t_sFMKNVM_PartitionStorageCfg
    c_PCSIM_FMKNVM_G4Storage_as[FMKNVM_PARTITION_NB] =
{
    {0U, 8192U, 2U},
    {8192U, 8192U, 2U},
    {16384U, 8192U, 2U}
};

/// @brief I2C EEPROM mapping with two 4-KiB slots per partition.
static const t_sFMKNVM_PartitionStorageCfg
    c_PCSIM_FMKNVM_I2cStorage_as[FMKNVM_PARTITION_NB] =
{
    {0U, 8192U, 2U},
    {8192U, 8192U, 2U},
    {16384U, 8192U, 2U}
};

/// @brief SPI EEPROM mapping with two 8-KiB slots per partition.
static const t_sFMKNVM_PartitionStorageCfg
    c_PCSIM_FMKNVM_SpiStorage_as[FMKNVM_PARTITION_NB] =
{
    {0U, 16384U, 2U},
    {16384U, 16384U, 2U},
    {32768U, 16384U, 2U}
};

/*********************************
 * FMKNVM_Specific_GetBackendApi
 *********************************/
const t_sFMKNVM_BackendApi * FMKNVM_Specific_GetBackendApi(void)
{
    const t_sFMKNVM_BackendApi * BackendApi_ps =
        &c_PCSIM_FMKNVM_BackendApi_s;

    //---- 1- Inject the PC simulator backend for every supported profile ----//

    return BackendApi_ps;
}

/*********************************
 * FMKNVM_Specific_GetPartitionStorage
 *********************************/
t_eReturnCode FMKNVM_Specific_GetPartitionStorage(   t_eFMKNVM_PartitionId f_PartitionId_e,
                                                     t_sFMKNVM_PartitionStorageCfg * f_StorageCfg_ps)
{
    const t_sFMKNVM_PartitionStorageCfg * StorageTable_pas = NULL;
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate the destination pointer and partition identifier ----//
    if(f_StorageCfg_ps == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(f_PartitionId_e >= FMKNVM_PARTITION_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        //---- 2- Select storage ranges matching the configured profile ----//
        switch(FMKNVM_EEPROM_TYPE)
        {
            case FMKNVM_EEPROM_TYPE_FLASH_H753:
            {
                StorageTable_pas = c_PCSIM_FMKNVM_H753Storage_as;
            }
            break;
            case FMKNVM_EEPROM_TYPE_FLASH_G4:
            {
                StorageTable_pas = c_PCSIM_FMKNVM_G4Storage_as;
            }
            break;
            case FMKNVM_EEPROM_TYPE_I2C:
            {
                StorageTable_pas = c_PCSIM_FMKNVM_I2cStorage_as;
            }
            break;
            case FMKNVM_EEPROM_TYPE_SPI:
            {
                StorageTable_pas = c_PCSIM_FMKNVM_SpiStorage_as;
            }
            break;
            case FMKNVM_EEPROM_TYPE_NB:
            default:
            {
                Ret_e = RC_ERROR_NOT_SUPPORTED;
            }
            break;
        }

        if(Ret_e == RC_OK)
        {
            //---- 3- Copy the selected mapping into the partition runtime ----//
            *f_StorageCfg_ps = StorageTable_pas[f_PartitionId_e];
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_Init
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_Init(void * f_Context_pv)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate and configure the selected simulator profile ----//
    if(f_Context_pv == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;

        Context_ps->isInitialized_b = FALSE;
        Ret_e = s_PCSIM_FMKNVM_ConfigureProfile(Context_ps);

        if(Ret_e == RC_OK)
        {
            //---- 2- Restore the persistent byte image from the host file ----//
            Ret_e = s_PCSIM_FMKNVM_LoadImage(Context_ps);
        }

        if(Ret_e == RC_OK)
        {
            Context_ps->isInitialized_b = TRUE;
        }

        Context_ps->lastResult_e = Ret_e;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_GetGeometry
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_GetGeometry(   void * f_Context_pv,
                                                   t_sFMKNVM_BackendGeometry * f_Geometry_ps)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate pointers and backend initialization ----//
    if((f_Context_pv == NULL) || (f_Geometry_ps == NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;

        if(Context_ps->isInitialized_b == FALSE)
        {
            Ret_e = RC_ERROR_INSTANCE_NOT_INITIALIZED;
        }
        else
        {
            //---- 2- Export the selected generic geometry ----//
            f_Geometry_ps->storageSize_u32 = Context_ps->storageSize_u32;
            f_Geometry_ps->eraseUnitSize_u32 =
                Context_ps->eraseUnitSize_u32;
            f_Geometry_ps->programUnitSize_u32 =
                Context_ps->programUnitSize_u32;
            f_Geometry_ps->commitUnitSize_u32 =
                Context_ps->commitUnitSize_u32;
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_GetCapabilities
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_GetCapabilities(   void * f_Context_pv,
                                                       t_sFMKNVM_BackendCapabilities * f_Capabilities_ps)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate pointers and backend initialization ----//
    if((f_Context_pv == NULL) || (f_Capabilities_ps == NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;

        if(Context_ps->isInitialized_b == FALSE)
        {
            Ret_e = RC_ERROR_INSTANCE_NOT_INITIALIZED;
        }
        else
        {
            //---- 2- Export behavior used by the generic transaction core ----//
            f_Capabilities_ps->eraseRequired_b =
                Context_ps->eraseRequired_b;
            f_Capabilities_ps->readWhileWriteSupported_b = TRUE;
            f_Capabilities_ps->maxTransferSize_u32 =
                Context_ps->pageSize_u32;
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_Read
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_Read(   void * f_Context_pv,
                                            t_uint32 f_Address_u32,
                                            t_uint8 * f_Data_pu8,
                                            t_uint32 f_Size_u32)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate pointers and initialization ----//
    if((f_Context_pv == NULL) || (f_Data_pu8 == NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;
        t_bool isRangeValid_b =
            s_PCSIM_FMKNVM_IsRangeValid( Context_ps,
                                         f_Address_u32,
                                         f_Size_u32);

        if(Context_ps->isInitialized_b == FALSE)
        {
            Ret_e = RC_ERROR_INSTANCE_NOT_INITIALIZED;
        }
        else if((isRangeValid_b == FALSE) ||
                (f_Size_u32 > 0xFFFFU))
        {
            Ret_e = RC_ERROR_PARAM_INVALID;
        }
        else
        {
            //---- 2- Copy bytes from the persistent RAM mirror ----//
            Ret_e = SafeMem_memcpy( f_Data_pu8,
                                    &g_PCSIM_FMKNVM_Storage_au8[
                                        f_Address_u32],
                                    (t_uint16)f_Size_u32);
        }

        Context_ps->lastResult_e = Ret_e;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_Program
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_Program(   void * f_Context_pv,
                                               t_uint32 f_Address_u32,
                                               const t_uint8 * f_Data_pcu8,
                                               t_uint32 f_Size_u32)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate pointers, range and Flash address alignment ----//
    if((f_Context_pv == NULL) || (f_Data_pcu8 == NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;
        t_bool isRangeValid_b =
            s_PCSIM_FMKNVM_IsRangeValid( Context_ps,
                                         f_Address_u32,
                                         f_Size_u32);

        if(Context_ps->isInitialized_b == FALSE)
        {
            Ret_e = RC_ERROR_INSTANCE_NOT_INITIALIZED;
        }
        else if((isRangeValid_b == FALSE) ||
                ((Context_ps->eraseRequired_b == TRUE) &&
                 ((f_Address_u32 %
                   Context_ps->programUnitSize_u32) != 0U)))
        {
            Ret_e = RC_ERROR_PARAM_INVALID;
        }
        else
        {
            t_uint32 processedSize_u32 = 0U;

            //---- 2- Split EEPROM writes at page boundaries ----//
            while((processedSize_u32 < f_Size_u32) &&
                  (Ret_e == RC_OK))
            {
                t_uint32 currentAddress_u32 =
                    f_Address_u32 + processedSize_u32;
                t_uint32 pageRemaining_u32 =
                    Context_ps->pageSize_u32 -
                    (currentAddress_u32 % Context_ps->pageSize_u32);
                t_uint32 chunkSize_u32 =
                    f_Size_u32 - processedSize_u32;

                if(chunkSize_u32 > pageRemaining_u32)
                {
                    chunkSize_u32 = pageRemaining_u32;
                }

                //---- 3- Apply Flash or EEPROM byte programming rules ----//
                for(t_uint32 idx_u32 = 0U ;
                    (idx_u32 < chunkSize_u32) &&
                    (Ret_e == RC_OK) ;
                    idx_u32++)
                {
                    t_uint32 storageIdx_u32 =
                        currentAddress_u32 + idx_u32;
                    t_uint8 source_u8 =
                        f_Data_pcu8[processedSize_u32 + idx_u32];

                    if(Context_ps->eraseRequired_b == TRUE)
                    {
                        t_uint8 current_u8 =
                            g_PCSIM_FMKNVM_Storage_au8[storageIdx_u32];
                        t_bool isTransitionValid_b =
                            ((current_u8 & source_u8) == source_u8);

                        if(isTransitionValid_b == FALSE)
                        {
                            Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
                        }
                        else
                        {
                            g_PCSIM_FMKNVM_Storage_au8[storageIdx_u32] =
                                current_u8 & source_u8;
                        }
                    }
                    else
                    {
                        g_PCSIM_FMKNVM_Storage_au8[storageIdx_u32] =
                            source_u8;
                    }
                }

                if(Ret_e == RC_OK)
                {
                    processedSize_u32 += chunkSize_u32;
                }
            }

            if(Ret_e == RC_OK)
            {
                //---- 4- Flush the programmed range for restart persistence ----//
                Ret_e = s_PCSIM_FMKNVM_PersistRange( Context_ps,
                                                     f_Address_u32,
                                                     f_Size_u32);
            }
        }

        Context_ps->lastResult_e = Ret_e;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_Erase
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_Erase(   void * f_Context_pv,
                                             t_uint32 f_Address_u32,
                                             t_uint32 f_Size_u32)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate context, range and selected erase alignment ----//
    if(f_Context_pv == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_FMKNVM_Context * Context_ps =
            (t_sPCSIM_FMKNVM_Context *)f_Context_pv;
        t_bool isRangeValid_b =
            s_PCSIM_FMKNVM_IsRangeValid( Context_ps,
                                         f_Address_u32,
                                         f_Size_u32);

        if(Context_ps->isInitialized_b == FALSE)
        {
            Ret_e = RC_ERROR_INSTANCE_NOT_INITIALIZED;
        }
        else if((isRangeValid_b == FALSE) ||
                (Context_ps->eraseRequired_b == FALSE) ||
                ((f_Address_u32 % Context_ps->eraseUnitSize_u32) != 0U) ||
                ((f_Size_u32 % Context_ps->eraseUnitSize_u32) != 0U))
        {
            Ret_e = RC_ERROR_PARAM_INVALID;
        }
        else
        {
            //---- 2- Restore the erased value across the complete range ----//
            for(t_uint32 idx_u32 = 0U ;
                idx_u32 < f_Size_u32 ;
                idx_u32++)
            {
                g_PCSIM_FMKNVM_Storage_au8[f_Address_u32 + idx_u32] =
                    0xFFU;
            }

            //---- 3- Persist the erased physical range ----//
            Ret_e = s_PCSIM_FMKNVM_PersistRange( Context_ps,
                                                 f_Address_u32,
                                                 f_Size_u32);

            if(Ret_e < RC_OK)
            {
                Ret_e = RC_NVM_BACKEND_ERASE_ERROR;
            }
        }

        Context_ps->lastResult_e = Ret_e;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_IsBusy
 *********************************/
static t_bool s_PCSIM_FMKNVM_IsBusy(void * f_Context_pv)
{
    t_bool isBusy_b = FALSE;

    //---- 1- Keep synchronous PC simulator operations immediately complete ----//
    (void)f_Context_pv;

    return isBusy_b;
}

/*********************************
 * s_PCSIM_FMKNVM_GetOperationResult
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_GetOperationResult(   void * f_Context_pv,
                                                          t_eReturnCode * f_OperationResult_pe)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate pointers and return the retained result ----//
    if((f_Context_pv == NULL) || (f_OperationResult_pe == NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        const t_sPCSIM_FMKNVM_Context * Context_ps =
            (const t_sPCSIM_FMKNVM_Context *)f_Context_pv;

        *f_OperationResult_pe = Context_ps->lastResult_e;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_Cyclic
 *********************************/
static void s_PCSIM_FMKNVM_Cyclic(void * f_Context_pv)
{
    //---- 1- No cyclic service is required for synchronous file accesses ----//
    (void)f_Context_pv;
}

/*********************************
 * s_PCSIM_FMKNVM_ConfigureProfile
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_ConfigureProfile(   t_sPCSIM_FMKNVM_Context * f_Context_ps)
{
    const char * EnvironmentPath_pc = getenv("PCSIM_NVM_FILE");
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Configure geometry and default image for the selected type ----//
    switch(FMKNVM_EEPROM_TYPE)
    {
        case FMKNVM_EEPROM_TYPE_FLASH_H753:
        {
            f_Context_ps->filePath_pc = "pcsim_fmknvm_h753.bin";
            f_Context_ps->storageSize_u32 = 1048576U;
            f_Context_ps->eraseUnitSize_u32 =
                PCSIM_FMKNVM_H753_ERASE_SIZE;
            f_Context_ps->programUnitSize_u32 =
                PCSIM_FMKNVM_H753_PROGRAM_SIZE;
            f_Context_ps->commitUnitSize_u32 =
                PCSIM_FMKNVM_H753_PROGRAM_SIZE;
            f_Context_ps->pageSize_u32 =
                PCSIM_FMKNVM_H753_PROGRAM_SIZE;
            f_Context_ps->eraseRequired_b = TRUE;
        }
        break;
        case FMKNVM_EEPROM_TYPE_FLASH_G4:
        {
            f_Context_ps->filePath_pc = "pcsim_fmknvm_g4.bin";
            f_Context_ps->storageSize_u32 = 24576U;
            f_Context_ps->eraseUnitSize_u32 =
                PCSIM_FMKNVM_G4_ERASE_SIZE;
            f_Context_ps->programUnitSize_u32 =
                PCSIM_FMKNVM_G4_PROGRAM_SIZE;
            f_Context_ps->commitUnitSize_u32 =
                PCSIM_FMKNVM_G4_PROGRAM_SIZE;
            f_Context_ps->pageSize_u32 =
                PCSIM_FMKNVM_G4_PROGRAM_SIZE;
            f_Context_ps->eraseRequired_b = TRUE;
        }
        break;
        case FMKNVM_EEPROM_TYPE_I2C:
        {
            f_Context_ps->filePath_pc = "pcsim_fmknvm_i2c.bin";
            f_Context_ps->storageSize_u32 = 24576U;
            f_Context_ps->eraseUnitSize_u32 = 1U;
            f_Context_ps->programUnitSize_u32 = 1U;
            f_Context_ps->commitUnitSize_u32 = 1U;
            f_Context_ps->pageSize_u32 = PCSIM_FMKNVM_I2C_PAGE_SIZE;
            f_Context_ps->eraseRequired_b = FALSE;
        }
        break;
        case FMKNVM_EEPROM_TYPE_SPI:
        {
            f_Context_ps->filePath_pc = "pcsim_fmknvm_spi.bin";
            f_Context_ps->storageSize_u32 = 49152U;
            f_Context_ps->eraseUnitSize_u32 = 1U;
            f_Context_ps->programUnitSize_u32 = 1U;
            f_Context_ps->commitUnitSize_u32 = 1U;
            f_Context_ps->pageSize_u32 = PCSIM_FMKNVM_SPI_PAGE_SIZE;
            f_Context_ps->eraseRequired_b = FALSE;
        }
        break;
        case FMKNVM_EEPROM_TYPE_NB:
        default:
        {
            Ret_e = RC_ERROR_NOT_SUPPORTED;
        }
        break;
    }

    if((Ret_e == RC_OK) &&
       (EnvironmentPath_pc != NULL) &&
       (EnvironmentPath_pc[0U] != '\0'))
    {
        //---- 2- Allow one persistent image path per simulated ECU ----//
        f_Context_ps->filePath_pc = EnvironmentPath_pc;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_LoadImage
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_LoadImage(   t_sPCSIM_FMKNVM_Context * f_Context_ps)
{
    FILE * ImageFile_ps;
    size_t readSize_sz = 0U;
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Initialize the complete RAM mirror to the erased value ----//
    for(t_uint32 idx_u32 = 0U ;
        idx_u32 < f_Context_ps->storageSize_u32 ;
        idx_u32++)
    {
        g_PCSIM_FMKNVM_Storage_au8[idx_u32] = 0xFFU;
    }

    //---- 2- Load an existing persistent image when available ----//
    ImageFile_ps = fopen(f_Context_ps->filePath_pc, "rb");

    if(ImageFile_ps != NULL)
    {
        readSize_sz = fread( g_PCSIM_FMKNVM_Storage_au8,
                             1U,
                             (size_t)f_Context_ps->storageSize_u32,
                             ImageFile_ps);
        (void)fclose(ImageFile_ps);
    }

    if(readSize_sz < (size_t)f_Context_ps->storageSize_u32)
    {
        //---- 3- Create or extend the image using erased trailing bytes ----//
        ImageFile_ps = fopen(f_Context_ps->filePath_pc, "wb");

        if(ImageFile_ps == NULL)
        {
            Ret_e = RC_NVM_BACKEND_READ_ERROR;
        }
        else
        {
            size_t writtenSize_sz =
                fwrite( g_PCSIM_FMKNVM_Storage_au8,
                        1U,
                        (size_t)f_Context_ps->storageSize_u32,
                        ImageFile_ps);
            int closeResult_s32 = fclose(ImageFile_ps);

            if((writtenSize_sz !=
                (size_t)f_Context_ps->storageSize_u32) ||
               (closeResult_s32 != 0))
            {
                Ret_e = RC_NVM_BACKEND_READ_ERROR;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_PersistRange
 *********************************/
static t_eReturnCode s_PCSIM_FMKNVM_PersistRange(   const t_sPCSIM_FMKNVM_Context * f_Context_ps,
                                                    t_uint32 f_Address_u32,
                                                    t_uint32 f_Size_u32)
{
    FILE * ImageFile_ps;
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Open the initialized persistent image for an in-place update ----//
    ImageFile_ps = fopen(f_Context_ps->filePath_pc, "r+b");

    if(ImageFile_ps == NULL)
    {
        Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
    }
    else
    {
        int seekResult_s32 =
            fseek(ImageFile_ps, (long)f_Address_u32, SEEK_SET);

        if(seekResult_s32 != 0)
        {
            Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
        }
        else
        {
            size_t writtenSize_sz =
                fwrite( &g_PCSIM_FMKNVM_Storage_au8[f_Address_u32],
                        1U,
                        (size_t)f_Size_u32,
                        ImageFile_ps);

            if(writtenSize_sz != (size_t)f_Size_u32)
            {
                Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
            }
        }

        if(Ret_e == RC_OK)
        {
            int flushResult_s32 = fflush(ImageFile_ps);

            if(flushResult_s32 != 0)
            {
                Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
            }
        }

        {
            int closeResult_s32 = fclose(ImageFile_ps);

            if((Ret_e == RC_OK) &&
               (closeResult_s32 != 0))
            {
                Ret_e = RC_NVM_BACKEND_PROGRAM_ERROR;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_FMKNVM_IsRangeValid
 *********************************/
static t_bool s_PCSIM_FMKNVM_IsRangeValid(   const t_sPCSIM_FMKNVM_Context * f_Context_ps,
                                             t_uint32 f_Address_u32,
                                             t_uint32 f_Size_u32)
{
    t_uint32 endAddress_u32 = f_Address_u32 + f_Size_u32;
    t_bool isRangeValid_b = FALSE;

    //---- 1- Reject empty, overflowing and out-of-component ranges ----//
    if((f_Size_u32 != 0U) &&
       (endAddress_u32 >= f_Address_u32) &&
       (endAddress_u32 <= f_Context_ps->storageSize_u32))
    {
        isRangeValid_b = TRUE;
    }

    return isRangeValid_b;
}
