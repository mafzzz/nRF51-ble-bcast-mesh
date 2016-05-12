/***********************************************************************************
Copyright (c) Nordic Semiconductor ASA
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  3. Neither the name of Nordic Semiconductor ASA nor the names of other
  contributors to this software may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************/

#include <string.h>
#include "bootloader_app_bridge.h"
#include "bl_if.h"
#include "dfu_mesh.h"
#include "bootloader_info.h"
#include "dfu_transfer_mesh.h"

/*****************************************************************************
* Local defines
*****************************************************************************/
#define LAST_RAM_WORD    (0x20007FFC)
/*****************************************************************************
* Local typedefs
*****************************************************************************/

/*****************************************************************************
* Static globals
*****************************************************************************/
uint32_t bl_cmd_handler(bl_cmd_t* p_bl_cmd); /**< Forward declaration of command handler */

static bl_if_cb_evt_t m_evt_handler = NULL;
/*****************************************************************************
* Static functions
*****************************************************************************/

/*****************************************************************************
* Interface functions
*****************************************************************************/
uint32_t bootloader_app_bridge_init(void)
{
    volatile bl_if_cmd_handler_t* p_last = (bl_if_cmd_handler_t*) LAST_RAM_WORD;
    *p_last = bl_cmd_handler;
    return NRF_SUCCESS;
}

uint32_t bootloader_evt_send(bl_evt_t* p_evt)
{
    if (m_evt_handler == NULL)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    return m_evt_handler(p_evt);
}

uint32_t bl_cmd_handler(bl_cmd_t* p_bl_cmd)
{
    bl_evt_t rsp_evt;
    switch (p_bl_cmd->type)
    {
        case BL_CMD_TYPE_INIT:
            if (p_bl_cmd->params.init.timer_count == 0 ||
                p_bl_cmd->params.init.tx_slots < 2)
            {
                return NRF_ERROR_INVALID_PARAM;
            }

            if (bootloader_info_init((uint32_t*) (BOOTLOADER_INFO_ADDRESS),
                                     (uint32_t*) (BOOTLOADER_INFO_ADDRESS - PAGE_SIZE))
                != NRF_SUCCESS)
            {
                send_abort_evt(BL_END_ERROR_INVALID_PERSISTENT_STORAGE);
                return NRF_ERROR_INTERNAL;
            }
            m_evt_handler = p_bl_cmd->params.init.event_callback;
            dfu_mesh_init(p_bl_cmd->params.init.tx_slots);
            break;
        case BL_CMD_TYPE_ENABLE:
            dfu_mesh_start();
            break;
        case BL_CMD_TYPE_RX:
            return dfu_mesh_rx(p_bl_cmd->params.rx.p_dfu_packet, p_bl_cmd->params.rx.length, true);
        case BL_CMD_TYPE_TIMEOUT:
            dfu_mesh_timeout();
            break;
        case BL_CMD_TYPE_ECHO:
            rsp_evt.type = BL_EVT_TYPE_ECHO;
            memcpy(rsp_evt.params.echo.str, p_bl_cmd->params.echo.str, 16);
            return m_evt_handler(&rsp_evt);

        case BL_CMD_TYPE_DFU_START_TARGET:

            break;
        case BL_CMD_TYPE_DFU_START_RELAY:
            break;
        case BL_CMD_TYPE_DFU_START_SOURCE:
            return NRF_ERROR_NOT_SUPPORTED;
        case BL_CMD_TYPE_DFU_ABORT:
            break;
        case BL_CMD_TYPE_DFU_FINALIZE:
            dfu_mesh_finalize();
            break;

        case BL_CMD_TYPE_INFO_GET:
            p_bl_cmd->params.info.get.p_entry =
                bootloader_info_entry_get((uint32_t*) BOOTLOADER_INFO_ADDRESS,
                                    p_bl_cmd->params.info.get.type);
            if (p_bl_cmd->params.info.get.p_entry == NULL)
            {
                return NRF_ERROR_NOT_FOUND;
            }
            break;
        case BL_CMD_TYPE_INFO_PUT:
            if (bootloader_info_entry_put(
                        p_bl_cmd->params.info.put.type,
                        p_bl_cmd->params.info.put.p_entry,
                        p_bl_cmd->params.info.put.length) < (bl_info_entry_t*) BOOTLOADER_INFO_ADDRESS)
            {
                return NRF_ERROR_INVALID_ADDR;
            }
            break;
        case BL_CMD_TYPE_INFO_ERASE:
            bootloader_info_entry_invalidate((uint32_t*) BOOTLOADER_INFO_ADDRESS,
                    p_bl_cmd->params.info.erase.type);
            break;
#if 0
        case BL_CMD_TYPE_UECC_SHARED_SECRET:
            if (!uECC_shared_secret(p_bl_cmd->params.uecc.shared_secret.p_public_key,
                                    p_bl_cmd->params.uecc.shared_secret.p_private_key,
                                    p_bl_cmd->params.uecc.shared_secret.p_secret,
                                    p_bl_cmd->params.uecc.shared_secret.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
        case BL_CMD_TYPE_UECC_MAKE_KEY:
            if (!uECC_make_key(p_bl_cmd->params.uecc.make_key.p_public_key,
                               p_bl_cmd->params.uecc.make_key.p_private_key,
                               p_bl_cmd->params.uecc.make_key.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
        case BL_CMD_TYPE_UECC_VALID_PUBLIC_KEY:
            if (!uECC_valid_public_key(p_bl_cmd->params.uecc.valid_public_key.p_public_key,
                                       p_bl_cmd->params.uecc.valid_public_key.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
        case BL_CMD_TYPE_UECC_COMPUTE_PUBLIC_KEY:
            if (!uECC_compute_public_key(p_bl_cmd->params.uecc.compute_public_key.p_private_key,
                                         p_bl_cmd->params.uecc.compute_public_key.p_public_key,
                                         p_bl_cmd->params.uecc.compute_public_key.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
        case BL_CMD_TYPE_UECC_SIGN:
            if (!uECC_sign(p_bl_cmd->params.uecc.sign.p_private_key,
                           p_bl_cmd->params.uecc.sign.p_hash,
                           p_bl_cmd->params.uecc.sign.hash_size,
                           p_bl_cmd->params.uecc.sign.p_signature,
                           p_bl_cmd->params.uecc.sign.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
        case BL_CMD_TYPE_UECC_VERIFY:
            if (!uECC_verify(p_bl_cmd->params.uecc.verify.p_public_key,
                             p_bl_cmd->params.uecc.verify.p_hash,
                             p_bl_cmd->params.uecc.verify.hash_size,
                             p_bl_cmd->params.uecc.verify.p_signature,
                             p_bl_cmd->params.uecc.verify.curve))
            {
                return NRF_ERROR_INTERNAL;
            }
            break;
#endif
        case BL_CMD_TYPE_FLASH_WRITE_COMPLETE:
            dfu_transfer_flash_write_complete(p_bl_cmd->params.flash.write.p_data);
            break;
        case BL_CMD_TYPE_FLASH_ERASE_COMPLETE:
            /* don't care */
            break;
        default:
            return NRF_ERROR_NOT_SUPPORTED;
    }

    return NRF_SUCCESS;
}

void send_abort_evt(bl_end_t end_reason)
{
    bl_evt_t abort_evt;
    abort_evt.type = BL_EVT_TYPE_ABORT;
    abort_evt.params.abort.reason = end_reason;
    bootloader_evt_send(&abort_evt);
}

uint32_t flash_write(uint32_t* p_dest, uint8_t* p_data, uint32_t length)
{
    NRF_GPIO->OUTSET = (1 << 2);
    uint32_t error_code;
    bl_evt_t evt =
    {
        .type = BL_EVT_TYPE_FLASH_WRITE,
        .params.flash.write =
        {
            .start_addr = (uint32_t) p_dest,
            .p_data = p_data,
            .length = length
        }
    };
    error_code = bootloader_evt_send(&evt);
    if (error_code == NRF_SUCCESS)
    {
        NRF_GPIO->OUTSET = (1 << 16);
        NRF_GPIO->OUTSET = (1 << 0);
    }
    NRF_GPIO->OUTCLR = (1 << 2);
    NRF_GPIO->OUTCLR = (1 << 16);
    return error_code;
}

uint32_t flash_erase(uint32_t* p_dest, uint32_t length)
{
    NRF_GPIO->OUTSET = (1 << 1);
    uint32_t error_code;
    bl_evt_t evt =
    {
        .type = BL_EVT_TYPE_FLASH_ERASE,
        .params.flash.erase =
        {
            .start_addr = (uint32_t) p_dest,
            .length = length
        }
    };
    error_code = bootloader_evt_send(&evt);
    if (error_code == NRF_SUCCESS)
    {
        NRF_GPIO->OUTSET = (1 << 0);
    }
    NRF_GPIO->OUTCLR = (1 << 1);
    return error_code;
}

uint32_t timer_set(uint32_t delay_us)
{
    bl_evt_t set_evt;
    set_evt.type = BL_EVT_TYPE_TIMER_SET;
    set_evt.params.timer.set.delay_us = delay_us;
    set_evt.params.timer.set.index = 0;
    return bootloader_evt_send(&set_evt);
}

uint32_t timer_abort(void)
{
    bl_evt_t abort_evt;
    abort_evt.type = BL_EVT_TYPE_TIMER_ABORT;
    abort_evt.params.timer.abort.index = 0;
    return bootloader_evt_send(&abort_evt);
}

uint32_t tx_abort(uint8_t slot)
{
    bl_evt_t abort_evt;
    abort_evt.type = BL_EVT_TYPE_TX_ABORT;
    abort_evt.params.tx.abort.tx_slot = slot;
    return bootloader_evt_send(&abort_evt);
}

uint32_t bootloader_error_post(uint32_t error, const char* file, uint32_t line)
{
    bl_evt_t error_evt;
    error_evt.type = BL_EVT_TYPE_ERROR;
    error_evt.params.error.error_code = error;
    error_evt.params.error.p_file = file;
    error_evt.params.error.line = line;
    return bootloader_evt_send(&error_evt);
}
