/*******************************************************************************
* File Name:   secure_tcp_client.c
*
* Description: This file contains task and functions related to secure TCP
* client operation.
*
********************************************************************************
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
/* Header file includes. */
#include "cybsp.h"
#include "retarget_io_init.h"

/* FreeRTOS header file. */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "cyabs_rtos.h"

/* Standard C header file. */
#include <string.h>

/* Cypress secure socket header file. */
#include "cy_secure_sockets.h"
#include "cy_tls.h"

/* Wi-Fi connection manager header files. */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/* Secure TCP client task header file. */
#include "secure_tcp_client.h"

/* IP address related header files (part of the lwIP TCP/IP stack). */
#include "ip_addr.h"

/* Wi-Fi credentials and TCP port settings header file. */
#include "network_credentials.h"

/* to use the portable formatting macros */
#include <inttypes.h>


/*******************************************************************************
* Macros
*******************************************************************************/
#define LED_ON_CMD                         '1'
#define LED_OFF_CMD                        '0'
#define ACK_LED_ON                         "LED ON ACK"
#define ACK_LED_OFF                        "LED OFF ACK"
#define MSG_INVALID_CMD                    "Invalid command"
#define RESET_VAL                          (0U)
#define CARRIAGE_RETURN                    ('\r')
#define NEWLINE                            ('\n')
#define BACKSPACE                          ('\b')
#define NULLCHARACTER                      ('\0')
#define BYTE_ZERO                          (0U)
#define APP_SDIO_INTERRUPT_PRIORITY        (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY   (2U)
#define APP_SDIO_FREQUENCY_HZ              (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK            (64U)


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
cy_rslt_t connect_to_secure_tcp_server(cy_socket_sockaddr_t address);
cy_rslt_t create_secure_tcp_client_socket(void);
cy_rslt_t tcp_client_recv_handler(cy_socket_t socket_handle, void *arg);
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg);

#if(USE_AP_INTERFACE)
    static cy_rslt_t softap_start(void);
#else
    static cy_rslt_t connect_to_wifi_ap(void);
#endif /* USE_AP_INTERFACE */

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* TLS credentials of the TCP client. */
static const char tcp_client_cert[] = keyCLIENT_CERTIFICATE_PEM;
static const char client_private_key[] = keyCLIENT_PRIVATE_KEY_PEM;

/* Root CA certificate for TCP server identity verification. */
static const char tcp_server_ca_cert[] = keySERVER_ROOTCA_PEM;

/* Variable to store the TLS identity (certificate and private key). */
void *tls_identity;

/* TCP client socket handle */
cy_socket_t client_handle;

/* Binary semaphore handle to keep track of secure TCP server connection. */
SemaphoreHandle_t connect_to_server;

/* Holds the IP address obtained for SoftAP using Wi-Fi Connection Manager (WCM). */
cy_wcm_ip_address_t softap_ip_address;
extern cy_stc_scb_uart_context_t    DEBUG_UART_context;
static mtb_hal_sdio_t sdio_instance;
static cy_stc_sd_host_context_t sdhc_host_context;
static cy_wcm_config_t wcm_config;


#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

/* SysPm callback parameter structure for SDHC */
static cy_stc_syspm_callback_params_t sdcardDSParams =
{
    .context   = &sdhc_host_context,
    .base      = CYBSP_WIFI_SDIO_HW
};

/* SysPm callback structure for SDHC*/
static cy_stc_syspm_callback_t sdhcDeepSleepCallbackHandler =
{
    .callback           = Cy_SD_Host_DeepSleepCallback,
    .skipMode           = SYSPM_SKIP_MODE,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &sdcardDSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};
#endif

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: sdio_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for SDIO instance.
*******************************************************************************/
static void sdio_interrupt_handler(void)
{
    mtb_hal_sdio_process_interrupt(&sdio_instance);
}

/*******************************************************************************
* Function Name: host_wake_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for the host wake up input pin.
*******************************************************************************/
static void host_wake_interrupt_handler(void)
{
    mtb_hal_gpio_process_interrupt(&wcm_config.wifi_host_wake_pin);
}

/*******************************************************************************
* Function Name: app_sdio_init
********************************************************************************
* Summary:
* This function configures and initializes the SDIO instance used in 
* communication between the host MCU and the wireless device.
*******************************************************************************/
static void app_sdio_init(void)
{
    cy_rslt_t result;
    mtb_hal_sdio_cfg_t sdio_hal_cfg;
    
    cy_stc_sysint_t sdio_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_SDIO_IRQ,
        .intrPriority = APP_SDIO_INTERRUPT_PRIORITY
    };

    cy_stc_sysint_t host_wake_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_HOST_WAKE_IRQ,
        .intrPriority = APP_HOST_WAKE_INTERRUPT_PRIORITY
    };

    /* Initialize the SDIO interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status = Cy_SysInt_Init(&sdio_intr_cfg,
            sdio_interrupt_handler);

    /* SDIO interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_SDIO_IRQ);

    /* Setup SDIO using the HAL object and desired configuration */
    result = mtb_hal_sdio_setup(&sdio_instance,
             &CYBSP_WIFI_SDIO_sdio_hal_config, NULL, &sdhc_host_context);

    /* SDIO setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Initialize and Enable SD HOST */
    Cy_SD_Host_Enable(CYBSP_WIFI_SDIO_HW);
    Cy_SD_Host_Init(CYBSP_WIFI_SDIO_HW, CYBSP_WIFI_SDIO_sdio_hal_config.
            host_config, &sdhc_host_context);
    Cy_SD_Host_SetHostBusWidth(CYBSP_WIFI_SDIO_HW, CY_SD_HOST_BUS_WIDTH_4_BIT);
    sdio_hal_cfg.frequencyhal_hz = APP_SDIO_FREQUENCY_HZ;
    sdio_hal_cfg.block_size = SDHC_SDIO_64BYTES_BLOCK;

    /* Configure SDIO */
    mtb_hal_sdio_configure(&sdio_instance, &sdio_hal_cfg);

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
    /* SDHC SysPm callback registration */
    Cy_SysPm_RegisterCallback(&sdhcDeepSleepCallbackHandler);
#endif /* (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP) */

    /* Setup GPIO using the HAL object for WIFI WL REG ON  */
    mtb_hal_gpio_setup(&wcm_config.wifi_wl_pin, CYBSP_WIFI_WL_REG_ON_PORT_NUM,
            CYBSP_WIFI_WL_REG_ON_PIN);

    /* Setup GPIO using the HAL object for WIFI HOST WAKE PIN  */
    mtb_hal_gpio_setup(&wcm_config.wifi_host_wake_pin,
            CYBSP_WIFI_HOST_WAKE_PORT_NUM, CYBSP_WIFI_HOST_WAKE_PIN);

    /* Initialize the Host wakeup interrupt and specify the interrupt handler.*/
    cy_en_sysint_status_t interrupt_init_status_host_wake =
            Cy_SysInt_Init(&host_wake_intr_cfg, host_wake_interrupt_handler);

    /* Host wake up interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status_host_wake)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_HOST_WAKE_IRQ);
}

/*******************************************************************************
* Function Name: uart_bytesAvailable
*******************************************************************************
* Summary:
*  Checks the number of bytes available to read from the receive buffers
*
* Parameters:
*  None
*
* Return:
*  uint32_t number_available: Return The number of readable bytes
*
*******************************************************************************/
static uint32_t uart_bytesAvailable(void)
{
    uint32_t numberofBytes_available = Cy_SCB_UART_GetNumInRxFifo(SCB2);
    if(DEBUG_UART_context.rxRingBuf != NULL)
    {
        numberofBytes_available += Cy_SCB_UART_GetNumInRingBuffer(SCB2,
                &DEBUG_UART_context);
    }
    return numberofBytes_available;
}

/*******************************************************************************
* Function Name: get_data_in_uart
*******************************************************************************
* Summary:
*  Reads the input data received from uart
*
* Parameters:
*  value : The value read from the serial port
*
* Return:
*  None
*******************************************************************************/
static void get_data_in_uart(uint8_t *value)
{
    uint32_t read_value = Cy_SCB_UART_Get(SCB2);
    while (CY_SCB_UART_RX_NO_DATA == read_value)
    {
        read_value = Cy_SCB_UART_Get(SCB2);
    }
    *value = (uint8_t)read_value;
}

/*******************************************************************************
* Function Name: read_uart_input
*******************************************************************************
* Summary:
*  Function to read user input from UART terminal.
*
* Parameters:
*  uint8_t* input_buffer_ptr: Pointer to input buffer
*
* Return:
*  None
*
*******************************************************************************/
static void read_uart_input(uint8_t* input_buffer_ptr)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t *input_read_buffer_ptr = input_buffer_ptr;
    uint32_t numBytes;

    do
    {
        /* Check for data in the UART buffer */
        numBytes = uart_bytesAvailable();

        if(numBytes > BYTE_ZERO)
        {
           get_data_in_uart(input_read_buffer_ptr);

            if(CY_RSLT_SUCCESS == result)
            {
                if((CARRIAGE_RETURN == *input_read_buffer_ptr) ||
                        (NEWLINE == *input_read_buffer_ptr))
                {
                    printf("\n");
                }
                else
                {
                    /* Echo the received character */
                    Cy_SCB_UART_Put(SCB2,*input_read_buffer_ptr);

                    if (BACKSPACE != *input_read_buffer_ptr)
                    {
                        input_read_buffer_ptr++;
                    }
                    else if(input_read_buffer_ptr != input_buffer_ptr)
                    {
                        input_read_buffer_ptr--;
                    }
                }
            }
        }
        cy_rtos_delay_milliseconds(RTOS_TICK_TO_WAIT);

    } while((*input_read_buffer_ptr != CARRIAGE_RETURN) &&
            (*input_read_buffer_ptr != NEWLINE));

    /* Terminate string with NULL character. */
    *input_read_buffer_ptr = NULLCHARACTER;
}

/*******************************************************************************
* Function Name: tcp_secure_client_task
*******************************************************************************
* Summary:
*  Task used to establish a secure connection to a remote TCP server and
*  control the LED state (ON/OFF) based on the command received from TCP server.
*
* Parameters:
*  void *args : Task parameter defined during task creation (unused)
*
* Return:
*  void
*
*******************************************************************************/
void tcp_secure_client_task(void *arg)
{
    cy_rslt_t result;
    uint8_t uart_input[UART_BUFFER_SIZE];

    /* IP address and TCP port number of the TCP server to which the TCP client
     * connects to. */
    cy_socket_sockaddr_t tcp_server_address = {
        #if(USE_IPV6_ADDRESS)
            .ip_address.ip.v6 =  TCP_SERVER_IP_ADDRESS,
            .ip_address.version = CY_SOCKET_IP_VER_V6,
        #else
            .ip_address.ip.v4 = TCP_SERVER_IP_ADDRESS,
            .ip_address.version = CY_SOCKET_IP_VER_V4,
        #endif
            .port = TCP_SERVER_PORT
    };

    app_sdio_init();
    wcm_config.interface = WIFI_INTERFACE_TYPE;
    wcm_config.wifi_interface_instance = &sdio_instance;

    /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wcm_config);

    if(CY_RSLT_SUCCESS != result)
    {
        printf("Wi-Fi Connection Manager initialization failed! Error code: "
                "0x%08"PRIx32"\n", (uint32_t)result);
        handle_app_error();
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

    #if(USE_AP_INTERFACE)

        /* Start the Wi-Fi device as a Soft AP interface. */
        result = softap_start();
        if (CY_RSLT_SUCCESS != result)
        {
            printf("Failed to Start Soft AP! Error code: 0x%08"PRIx32"\n",
                    (uint32_t)result);
            handle_app_error();
        }
    #else
        /* Connect to Wi-Fi AP */
        if(CY_RSLT_SUCCESS != connect_to_wifi_ap())
        {
            printf("\n Failed to connect to Wi-Fi AP.\n");
            handle_app_error();
        }
    #endif /* USE_AP_INTERFACE */

    /* Create a binary semaphore to keep track of secure TCP server connection. */
    connect_to_server = xSemaphoreCreateBinary();

    /* Give the semaphore so as to connect to TCP server. */
    xSemaphoreGive(connect_to_server); 

    /* TCP client certificate length and private key length. */
    const size_t tcp_client_cert_len = strlen( tcp_client_cert );
    const size_t pkey_len = strlen( client_private_key );

    /* Initialize secure socket library. */
    result = cy_socket_init();
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Secure Socket initialization failed!\n");
        handle_app_error();
    }
    printf("Secure Socket initialized\n");

    /* Initializes the global trusted RootCA certificate. This examples uses a
     * self signed certificate which implies that the RootCA certificate is same
     * as the certificate of TCP secure server to which client is connecting to.
     */
    result = cy_tls_load_global_root_ca_certificates(tcp_server_ca_cert,
            strlen(tcp_server_ca_cert));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("cy_tls_load_global_root_ca_certificates failed! Error code: "
                "%"PRIu32"\n", result);
    }
    else
    {
        printf("Global trusted RootCA certificate loaded\n");
    }

    /* Create TCP client identity using the SSL certificate and private key. */
    result = cy_tls_create_identity(tcp_client_cert, tcp_client_cert_len,
                                    client_private_key, pkey_len, &tls_identity);
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Failed cy_tls_create_identity! Error code: %"PRIu32"\n", result);
       handle_app_error();
    }   

    for(;;)
    {
        /* Wait till semaphore is acquired so as to connect to a secure
         * TCP server. */
        xSemaphoreTake(connect_to_server, portMAX_DELAY);

        printf("Connect to TCP server\n");

        #if(USE_IPV6_ADDRESS)
            printf("Enter the IPv6 address of the TCP Server:\n");
        #else
            printf("Enter the IPv4 address of the TCP Server:\n");
        #endif
            
        /* Clear the UART input buffer. */
        memset(uart_input, RESET_VAL, UART_BUFFER_SIZE);

        /* Read the TCP server's IPv4 address from  the user via the
         * UART terminal.
         */
        read_uart_input(uart_input);

        #if(USE_IPV6_ADDRESS)
            ip6addr_aton((char *)uart_input,
                         (ip6_addr_t *)&tcp_server_address.ip_address.ip.v6);
        #else
            ip4addr_aton((char *)uart_input,
                         (ip4_addr_t *)&tcp_server_address.ip_address.ip.v4);
        #endif

        #if(USE_IPV6_ADDRESS)
            printf("Connecting to TCP Server (IPv6 Address: %s, Port: %d)\n\n",
                    ip6addr_ntoa((const ip6_addr_t *)&tcp_server_address.
                            ip_address.ip.v6),
                    TCP_SERVER_PORT);
        #else
            printf("Connecting to TCP Server (IPv4 Address: %s, Port: %d)\n\n",
                    ip4addr_ntoa((const ip4_addr_t *)&tcp_server_address.
                            ip_address.ip.v4),
                    TCP_SERVER_PORT);
        #endif

        /* Connect to the secure TCP server. If the connection fails, retry
         * to connect to the server for MAX_TCP_SERVER_CONN_RETRIES times. */
        printf("Connecting to TCP server...\n");
        result = connect_to_secure_tcp_server(tcp_server_address);

        if (CY_RSLT_SUCCESS != result)
        {
            printf("Failed to connect to TCP server. Error code: %"PRIu32"\n",
                    result);

            /* Give the semaphore so as to connect to TCP server. */
            xSemaphoreGive(connect_to_server);
        }
    }
 }

#if(USE_AP_INTERFACE)
/********************************************************************************
* Function Name: softap_start
********************************************************************************
* Summary:
*  This function configures device in AP mode and initializes
*  a SoftAP with the given credentials (SOFTAP_SSID, SOFTAP_PASSWORD and
*  SOFTAP_SECURITY_TYPE).
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: Returns CY_RSLT_SUCCESS if the Soft AP is started successfully,
*  a WCM error code otherwise.
*
*******************************************************************************/
static cy_rslt_t softap_start(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    #if(USE_IPV6_ADDRESS)
        cy_wcm_ip_address_t ip_address;
    #endif 

    /* Initialize the Wi-Fi device as a Soft AP. */
    cy_wcm_ap_credentials_t softap_credentials = {SOFTAP_SSID, SOFTAP_PASSWORD,
                                                  SOFTAP_SECURITY_TYPE};

    static const cy_wcm_ip_setting_t softap_ip_info = {
        INITIALISER_IPV4_ADDRESS(.ip_address, SOFTAP_IP_ADDRESS),
        INITIALISER_IPV4_ADDRESS(.gateway,    SOFTAP_GATEWAY),
        INITIALISER_IPV4_ADDRESS(.netmask,    SOFTAP_NETMASK)
        };

    cy_wcm_wifi_band_t wifi_band ={CY_WCM_WIFI_BAND_ANY};
    cy_wcm_ap_config_t softap_config = {softap_credentials, wifi_band,
                                        SOFTAP_RADIO_CHANNEL,
                                        softap_ip_info,
                                        NULL};

    /* Start the the Wi-Fi device as a Soft AP. */
    result = cy_wcm_start_ap(&softap_config);

    if (CY_RSLT_SUCCESS == result)
    {
        printf("Wi-Fi Device configured as Soft AP\n");
        printf("Connect TCP client device to the network: SSID: %s Password:%s\n",
                SOFTAP_SSID, SOFTAP_PASSWORD);
    #if(USE_IPV6_ADDRESS)
        /* Get the IPv6 address.*/
        result = cy_wcm_get_ipv6_addr(CY_WCM_INTERFACE_TYPE_AP,
                 CY_WCM_IPV6_LINK_LOCAL, &ip_address);
        if (CY_RSLT_SUCCESS == result)
        {
            printf("SofAP IPv6 Address : %s\n\n",
                   ip6addr_ntoa((const ip6_addr_t*)&ip_address.ip.v6));
        }
    #else
        printf("SofAP IPv4 Address : %s\n\n",
                ip4addr_ntoa((const ip4_addr_t *)&softap_ip_info.
                        ip_address.ip.v4));
    #endif  
    }

    return result;
}
#endif /* USE_AP_INTERFACE */

#if(!USE_AP_INTERFACE)
/*******************************************************************************
* Function Name: connect_to_wifi_ap()
*******************************************************************************
* Summary:
*  Connects to Wi-Fi AP using the user-configured credentials, retries up to a
*  configured number of times until the connection succeeds.
*
* Parameters:
*  void
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if the Wi-Fi AP connection is
*                    successful.
*
*******************************************************************************/
cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;

    /* Variables used by Wi-Fi connection manager. */
    cy_wcm_connect_params_t wifi_conn_param;
    cy_wcm_ip_address_t ip_address;

     /* Set the Wi-Fi SSID, password and security type. */
    memset(&wifi_conn_param, RESET_VAL, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password, WIFI_PASSWORD,
            sizeof(WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = WIFI_SECURITY_TYPE;

    /* Join the Wi-Fi AP. */
    for(uint32_t conn_retries = RESET_VAL; conn_retries < MAX_WIFI_CONN_RETRIES;
            conn_retries++ )
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);
        if (CY_RSLT_SUCCESS == result)
        {
            printf("Successfully connected to Wi-Fi network '%s'.\n",
                                wifi_conn_param.ap_credentials.SSID);

            #if(USE_IPV6_ADDRESS)
            /* Get the IPv6 address.*/
                result = cy_wcm_get_ipv6_addr(CY_WCM_INTERFACE_TYPE_STA,
                                              CY_WCM_IPV6_LINK_LOCAL, &ip_address);
                if (CY_RSLT_SUCCESS == result)
                {
                    printf("IPv6 address (link-local) assigned: %s\n",
                            ip6addr_ntoa((const ip6_addr_t*)&ip_address.ip.v6));
                }
            #else      
                printf("IPv4 address assigned: %s\n",
                        ip4addr_ntoa((const ip4_addr_t*)&ip_address.ip.v4));
       
            #endif /* USE_IPV6_ADDRESS */
            return result;
        }
        printf("Connection to Wi-Fi network failed with error code %"PRIu32"."
               "Retrying in %d ms...\n", result, WIFI_CONN_RETRY_INTERVAL_MSEC);
        vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MSEC));
    }

    /* Stop retrying after maximum retry attempts. */
    printf("Exceeded maximum Wi-Fi connection attempts\n");
    return result;
}
#endif /* (!USE_AP_INTERFACE)*/

/*******************************************************************************
* Function Name: create_secure_tcp_client_socket
*******************************************************************************
* Summary:
*  Function to create a secure socket and set the socket options to use TLS
*  identity, set call back function for handling incoming messages, call back
*  function to handle disconnection.
*
* Parameters:
*  void
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if secure socket is created
*                    successfully.
*
*******************************************************************************/
cy_rslt_t create_secure_tcp_client_socket(void)
{
    cy_rslt_t result;

    /* Variables used to set socket options. */
    cy_socket_opt_callback_t tcp_recv_option;
    cy_socket_opt_callback_t tcp_disconnection_option;

    /* TLS authentication mode. */
    cy_socket_tls_auth_mode_t tls_auth_mode = CY_SOCKET_TLS_VERIFY_REQUIRED;

    /* Create a new secure TCP socket. */
    #if(USE_IPV6_ADDRESS)
        result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET6, CY_SOCKET_TYPE_STREAM,
                              CY_SOCKET_IPPROTO_TLS, &client_handle);
    #else
        result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM,
                                      CY_SOCKET_IPPROTO_TLS, &client_handle);
    #endif

    if (CY_RSLT_SUCCESS != result)
    {
        printf("Failed to create socket! Error Code: %"PRIu32"\n", result);
        return result;
    }

    /* Register the callback function to handle messages received from TCP server. */
    tcp_recv_option.callback = tcp_client_recv_handler;
    tcp_recv_option.arg = NULL;
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                  CY_SOCKET_SO_RECEIVE_CALLBACK,
                                  &tcp_recv_option, sizeof(cy_socket_opt_callback_t));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_RECEIVE_CALLBACK failed! "
                "Error Code: %"PRIu32"\n", result);
        return result;
    }

    /* Register the callback function to handle disconnection. */
    tcp_disconnection_option.callback = tcp_disconnection_handler;
    tcp_disconnection_option.arg = NULL;
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_SOCKET,
                                  CY_SOCKET_SO_DISCONNECT_CALLBACK,
                                  &tcp_disconnection_option,
                                  sizeof(cy_socket_opt_callback_t));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_DISCONNECT_CALLBACK failed! "
                "Error Code: %"PRIu32"\n", result);
        return result;
    }

    /* Set the TCP socket to use the TLS identity. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TLS,
                                  CY_SOCKET_SO_TLS_IDENTITY,
                                  tls_identity, sizeof((uint32_t)tls_identity));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TLS_IDENTITY failed! "
               "Error Code: %"PRIu32"\n", result);
    }

    /* Set the TLS authentication mode. */
    result = cy_socket_setsockopt(client_handle, CY_SOCKET_SOL_TLS,
                        CY_SOCKET_SO_TLS_AUTH_MODE,
                        &tls_auth_mode, sizeof(cy_socket_tls_auth_mode_t));
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Set socket option: CY_SOCKET_SO_TLS_AUTH_MODE failed! "
               "Error Code: %"PRIu32"\n", result);
    }
    return result;
}

/*******************************************************************************
* Function Name: connect_to_secure_tcp_server
*******************************************************************************
* Summary:
*  Function to connect to secure TCP server.
*
* Parameters:
*  cy_socket_sockaddr_t address: Address of TCP server socket
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if connection is successful.
*
*******************************************************************************/
cy_rslt_t connect_to_secure_tcp_server(cy_socket_sockaddr_t address)
{
    cy_rslt_t result = CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT;
    cy_rslt_t conn_result;  

    for(uint32_t conn_retries = RESET_VAL; conn_retries <
                 MAX_TCP_SERVER_CONN_RETRIES; conn_retries++)
    {
        /* Create a secure TCP socket */
        conn_result = create_secure_tcp_client_socket();
        if(CY_RSLT_SUCCESS != conn_result )
        {
            printf("Failed to create secure socket! Error Code: "
                    "%"PRIu32"\n", result);
           handle_app_error();
        }
        
        conn_result = cy_socket_connect(client_handle, &address,
                sizeof(cy_socket_sockaddr_t));
        if (CY_RSLT_SUCCESS == conn_result)
        {
            printf("======================================================"
                    "======\n");
            printf("TLS Handshake successful and connected to TCP server\n");
            return conn_result;
        }

        printf("Could not connect to TCP server.\n");
        printf("Trying to reconnect to TCP server...Please check if server is "
                "listening\n");

        /* The resources allocated during the socket creation (cy_socket_create)
         * should be deleted.
         */
        cy_socket_delete(client_handle);
    }

     /* Stop retrying after maximum retry attempts. */
     printf("Exceeded maximum connection attempts to the TCP server\n");

     return result;
}

/*******************************************************************************
* Function Name: tcp_client_recv_handler
********************************************************************************
* Summary:
*  Callback function to handle incoming TCP server messages.
*
* Parameters:
*  cy_socket_t socket_handle: Connection handle for the TCP client socket
*  void *args : Parameter passed on to the function (unused)
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if acknowledgement to the secure
*                    TCP server is sent in receipt of the message received.
*
*******************************************************************************/
cy_rslt_t tcp_client_recv_handler(cy_socket_t socket_handle, void *arg)
{
    /* Variable to store number of bytes send to the TCP server. */
    uint32_t bytes_sent = RESET_VAL;

    /* Variable to store number of bytes received. */
    uint32_t bytes_received = RESET_VAL;

    char message_buffer[MAX_TCP_DATA_PACKET_LENGTH];
    cy_rslt_t result ;

    printf("============================================================\n");
    result = cy_socket_recv(socket_handle, message_buffer, TCP_LED_CMD_LEN,
                            CY_SOCKET_FLAGS_NONE, &bytes_received);
    if(CY_RSLT_SUCCESS != result)
    {
        return result;
    }

    if(LED_ON_CMD == message_buffer[0])
    {
        /* Turn the LED ON. */
        Cy_GPIO_Write(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, CYBSP_LED_STATE_ON);
        printf("LED turned ON\n");
        sprintf(message_buffer, ACK_LED_ON);
    }
    else if(LED_OFF_CMD == message_buffer[0])
    {
        /* Turn the LED OFF. */
        Cy_GPIO_Write(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, CYBSP_LED_STATE_OFF);

        printf("LED turned OFF\n");
        sprintf(message_buffer, ACK_LED_OFF);
    }
    else
    {
        printf("Invalid command\n");
        sprintf(message_buffer, MSG_INVALID_CMD);
    }

    /* Send acknowledgement to the secure TCP server in receipt of the message
     * received. */
    result = cy_socket_send(socket_handle, message_buffer, strlen(message_buffer),
                            CY_SOCKET_FLAGS_NONE, &bytes_sent);
    if(CY_RSLT_SUCCESS == result)
    {
        printf("Acknowledgement sent to TCP server\n");
    }
    return result;
}

/*******************************************************************************
* Function Name: tcp_disconnection_handler
*******************************************************************************
* Summary:
*  Callback function to handle TCP client disconnection event.
*
* Parameters:
* cy_socket_t socket_handle: Connection handle for the TCP client socket
*  void *args : Parameter passed on to the function (unused)
*
* Return:
*  cy_result result: Returns CY_RSLT_SUCCESS if disconnection is successful.
*
*******************************************************************************/
cy_rslt_t tcp_disconnection_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result;

    /* Disconnect the TCP client. */
    result = cy_socket_disconnect(socket_handle, RESET_VAL);
    
    /* Free the resources allocated to the socket. */
    cy_socket_delete(socket_handle);

    printf("Disconnected from the TCP server! \n");

    /* Give the semaphore so as to connect to TCP server.  */
    xSemaphoreGive(connect_to_server);

    return result;
}

/* [] END OF FILE */
