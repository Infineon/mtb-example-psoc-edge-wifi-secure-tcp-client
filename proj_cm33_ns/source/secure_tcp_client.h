/******************************************************************************
* File Name:   secure_tcp_client.h
*
* Description: This file contains declaration of task related to secure TCP
* client operation.
*
*******************************************************************************
* (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Include Guard
*******************************************************************************/
#ifndef SECURE_TCP_CLIENT_H_
#define SECURE_TCP_CLIENT_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Macros
*******************************************************************************/
/* Maximum number of connection retries to the TCP server. */
#define MAX_TCP_SERVER_CONN_RETRIES           (5U)

/* Length of the TCP data packet. */
#define MAX_TCP_DATA_PACKET_LENGTH            (20U)

/* Length of the LED ON/OFF command issued from the TCP server. */
#define TCP_LED_CMD_LEN                       (1U)

/* Set this macro to '1' to enable IPv6 protocol. Default value is '0' to use
 * IPv4 protocol.
 */
#define USE_IPV6_ADDRESS                      (0U)
#define TCP_SERVER_PORT                       (50007U)
#define RTOS_TICK_TO_WAIT                     (1U)
#define UART_INPUT_TIMEOUT_MS                 (1U)
#define UART_BUFFER_SIZE                      (50U)

/*******************************************************************************
* Function Prototype
*******************************************************************************/
void tcp_secure_client_task(void *arg);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SECURE_TCP_CLIENT_H_ */
