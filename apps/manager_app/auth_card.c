
/**
 * @file    auth_card.c
 * @author  Cypherock X1 Team
 * @brief   Card authentication flow module
 *          Short description of the file
 * @copyright Copyright (c) 2023 HODL TECH PTE LTD
 * <br/> You may obtain a copy of license at <a href="https://mitcc.org/"
 *target=_blank>https://mitcc.org/</a>
 *
 ******************************************************************************
 * @attention
 *
 * (c) Copyright 2023 by HODL TECH PTE LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * "Commons Clause" License Condition v1.0
 *
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition.
 *
 * Without limiting other conditions in the License, the grant of
 * rights under the License will not include, and the License does not
 * grant to you, the right to Sell the Software.
 *
 * For purposes of the foregoing, "Sell" means practicing any or all
 * of the rights granted to you under the License to provide to third
 * parties, for a fee or other consideration (including without
 * limitation fees for hosting or consulting/ support services related
 * to the Software), a product or service whose value derives, entirely
 * or substantially, from the functionality of the Software. Any license
 * notice or attribution required by the License must also include
 * this Commons Clause License Condition notice.
 *
 * Software: All X1Wallet associated files.
 * License: MIT
 * Licensor: HODL TECH PTE LTD
 *
 ******************************************************************************
 */

/*****************************************************************************
 * INCLUDES
 *****************************************************************************/
#include "controller_tap_cards.h"
#include "events.h"
#include "lv_task.h"
#include "manager_api.h"
#include "manager_app.h"
#include "manager_app_priv.h"
#include "nfc.h"
#include "status_api.h"
#include "ui_confirmation.h"
#include "ui_delay.h"
#include "ui_instruction.h"

/*****************************************************************************
 * EXTERN VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * PRIVATE MACROS AND DEFINES
 *****************************************************************************/
#define HANDLE_P0_EVENTS()
#define CHALLENGE_SIZE 32

#define SIGN_SERIAL_BEEP_COUNT(pair_card_required) (pair_card_required) ? 3 : 2
#define SIGN_CHALLENGE_BEEP_COUNT(pair_card_required)                          \
  (pair_card_required) ? 2 : 1
/*****************************************************************************
 * PRIVATE TYPEDEFS
 *****************************************************************************/
typedef struct {
  char heading[30];
  char message[100];
  uint8_t acceptable_cards;
  bool pair_card_required;
} auth_card_screen_ctx_t;

typedef struct {
  manager_query_t query;
  auth_card_screen_ctx_t ctx;
} auth_card_data_t;
/*****************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *****************************************************************************/

/**
 * @brief If usb event is present, parses and verifies the query and appends to
 * query_out object
 *
 * @param usb_evt usb event object
 * @param query_out query object for caller
 * @return manager_error_code_t MANAGER_TASK_SUCCESS
 */
static manager_error_code_t decode_and_verify_auth_card_query_if_usb_evt(
    usb_event_t *usb_evt,
    manager_query_t *query_out);

/**
 * @brief Helper function to send byte stream encoded auth card responses to
 * host
 *
 * @param resp auth card response object with response type and relevant data
 * @return manager_error_code_t MANAGER_TASK_SUCCESS sent encoded data
 * successfully, else error return
 */
static manager_error_code_t send_auth_card_response(
    manager_auth_card_response_t *resp);

/**
 * @brief Read data from initiate request and prepare context for card
 * authentication
 *
 * @param auth_card_data object of auth card data
 */
static void prepare_card_auth_context(auth_card_data_t *auth_card_data);

/**
 * @brief Helper to return signature of card serial number.
 * Handles serial signing task and populates resp object with appropriate
 * response
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_sign_card_serial(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp);

/**
 * @brief Helper to return signature of server challenge.
 * Handles challenge signing task and populates resp object with appropriate
 * response
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_sign_challenge(auth_card_data_t *auth_card_data,
                                           manager_auth_card_response_t *resp);

/**
 * @brief Helper to handle initiate query for auth card.
 * Hanlder performs following actions
 * - Get user's confirmation for proceeding with card auth
 * - Prepare context for auth card flow
 * - returns sign serial response
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_auth_card_initiate_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp);

/**
 * @brief Helper to handle challenge query for auth card.
 * Handles challenge signing task and populates resp object with appropriate
 * response
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_auth_card_challenge_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp);

/**
 * @brief Helper to handle result of auth card.
 * Handles pairing of card based on result, flow state and card pair requirement
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_auth_card_result_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp);

/**
 * @brief Helper to parse handle queries for auth card.
 * Hanlder performs following actions
 * - Decode host query for manager app
 * - Verify query is for card auth and type of query
 * - Call relevant handler for queries
 *
 * @param auth_card_data object of auth card data
 * @param resp object pointer card auth response
 * @return manager_error_code_t MANAGER_TASK_SUCCESS if successful, else error
 * code
 */
manager_error_code_t handle_auth_card_query(auth_card_data_t *auth_card_data,
                                            manager_auth_card_response_t *resp);
/*****************************************************************************
 * STATIC VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * GLOBAL VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * STATIC FUNCTIONS
 *****************************************************************************/
static manager_error_code_t decode_and_verify_auth_card_query_if_usb_evt(
    usb_event_t *usb_evt,
    manager_query_t *query_out) {
  if (NULL == usb_evt || 0 == query_out) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  if (false == usb_evt->flag) {
    // No USB event so don't throw error, if P0 occured, will be handled by
    // caller
    return MANAGER_TASK_SUCCESS;
  }

  manager_query_t query = MANAGER_QUERY_INIT_ZERO;

  if (false ==
      decode_manager_query(usb_evt->p_msg, usb_evt->msg_size, &query)) {
    return MANAGER_TASK_DECODING_FAILED;
  }

  if ((query.which_request != MANAGER_QUERY_AUTH_CARD_TAG)) {
    return MANAGER_TASK_UNKNOWN_QUERY_REQUEST;
  }

  memcpy(query_out, &query, sizeof(query));
  return MANAGER_TASK_SUCCESS;
}

static manager_error_code_t send_auth_card_response(
    manager_auth_card_response_t *resp) {
  if (NULL == resp || 0 == resp->which_response) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  // Initialize respose with zero values
  manager_result_t result = MANAGER_RESULT_INIT_ZERO;

  /* TODO: Update the buffer size when all option files have been updated for
   * manager app*/
  uint8_t buffer[MANAGER_AUTH_CARD_RESPONSE_SIZE + 4] = {0};
  size_t bytes_written = 0;

  // Set auth card valid tags
  result.which_response = MANAGER_QUERY_AUTH_CARD_TAG;

  // Copy response to pb result struct
  memcpy(&(result.auth_card), resp, sizeof(manager_auth_card_response_t));

  // Encode result to byte-stream
  if (false ==
      encode_manager_result(&result, buffer, sizeof(buffer), &bytes_written)) {
    // Encoding failed, return false
    return MANAGER_TASK_ENCODING_FAILED;
  }

  usb_send_msg(buffer, bytes_written);
  return MANAGER_TASK_SUCCESS;
}

static void prepare_card_auth_context(auth_card_data_t *auth_card_data) {
  auth_card_data->ctx.acceptable_cards = ACCEPTABLE_CARDS_ALL;

  if (auth_card_data->query.auth_card.initiate.has_card_index) {
    auth_card_data->ctx.acceptable_cards =
        encode_card_number(auth_card_data->query.auth_card.initiate.card_index);
    snprintf(auth_card_data->ctx.heading,
             sizeof(auth_card_data->ctx.heading),
             UI_TEXT_TAP_CARD,
             (uint8_t)auth_card_data->query.auth_card.initiate.card_index);
  } else {
    snprintf(auth_card_data->ctx.heading,
             sizeof(auth_card_data->ctx.heading),
             ui_text_tap_a_card);
  }

  auth_card_data->ctx.pair_card_required = false;
  if (auth_card_data->query.auth_card.initiate.has_pair_card) {
    auth_card_data->ctx.pair_card_required =
        auth_card_data->query.auth_card.initiate.pair_card;
  }
  snprintf(auth_card_data->ctx.message,
           sizeof(auth_card_data->ctx.message),
           UI_TEXT_PLACE_CARD_TILL_BEEP,
           SIGN_SERIAL_BEEP_COUNT(auth_card_data->ctx.pair_card_required));
}

manager_error_code_t handle_sign_card_serial(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data || NULL == resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  instruction_scr_init(auth_card_data->ctx.message,
                       auth_card_data->ctx.heading);
  nfc_en_select_card_task();

  evt_status_t status = get_events(EVENT_CONFIG_NFC, MAX_INACTIVITY_TIMEOUT);

  if (true == status.p0_event.flag) {
    HANDLE_P0_EVENTS();
    return MANAGER_TASK_P0_ABORT_OCCURED;
  }

  // NFC event occured

  /* TODO: Add sign serial card action*/

  /* TODO: Check if card operation failed*/
  // return MANAGER_TASK_FAILED;

  core_status_set_flow_status(MANAGER_AUTH_CARD_STATUS_SERIAL_SIGNED);

  // Display text for challenge sign screen
  snprintf(auth_card_data->ctx.message,
           sizeof(auth_card_data->ctx.message),
           UI_TEXT_PLACE_CARD_TILL_BEEP,
           SIGN_CHALLENGE_BEEP_COUNT(auth_card_data->ctx.pair_card_required));
  instruction_scr_init(auth_card_data->ctx.message,
                       auth_card_data->ctx.heading);

  resp->which_response = MANAGER_AUTH_CARD_RESPONSE_SERIAL_SIGNATURE_TAG;
  return MANAGER_TASK_SUCCESS;
}

manager_error_code_t handle_sign_challenge(auth_card_data_t *auth_card_data,
                                           manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data || NULL == resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  nfc_en_select_card_task();

  evt_status_t status = get_events(EVENT_CONFIG_NFC, MAX_INACTIVITY_TIMEOUT);

  if (true == status.p0_event.flag) {
    HANDLE_P0_EVENTS();
    return MANAGER_TASK_P0_ABORT_OCCURED;
  }

  // NFC event occured

  /* TODO: Add sign challenge card action*/

  /* TODO: Check if card operation failed*/
  // return MANAGER_TASK_FAILED;

  core_status_set_flow_status(MANAGER_AUTH_CARD_STATUS_CHALLENGE_SIGNED);

  if (auth_card_data->ctx.pair_card_required) {
    snprintf(auth_card_data->ctx.message,
             sizeof(auth_card_data->ctx.message),
             UI_TEXT_PLACE_CARD_TILL_BEEP,
             1);
  } else {
    snprintf(auth_card_data->ctx.message,
             sizeof(auth_card_data->ctx.message),
             "...");
  }

  instruction_scr_init(auth_card_data->ctx.message,
                       auth_card_data->ctx.heading);
  resp->which_response = MANAGER_AUTH_CARD_RESPONSE_CHALLENGE_SIGNATURE_TAG;
  return MANAGER_TASK_SUCCESS;
}

manager_error_code_t handle_auth_card_initiate_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data || NULL == resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  if (MANAGER_AUTH_CARD_STATUS_INIT != core_status_get_flow_status()) {
    return MANAGER_TASK_INVALID_STATE;
  }

  /* TODO: If onboarding is done, add check step for confirmation */
  // if(false == wait_for_user_confirmation()){
  //   //Abort operations
  //    return MANAGER_TASK_REJECTED;
  // }

  core_status_set_flow_status(MANAGER_AUTH_CARD_STATUS_USER_CONFIRMED);
  prepare_card_auth_context(auth_card_data);
  return handle_sign_card_serial(auth_card_data, resp);
}

manager_error_code_t handle_auth_card_challenge_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data || NULL == resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  if (MANAGER_AUTH_CARD_STATUS_SERIAL_SIGNED != core_status_get_flow_status()) {
    return MANAGER_TASK_INVALID_STATE;
  }

  return handle_sign_challenge(auth_card_data, resp);
}

manager_error_code_t handle_auth_card_result_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data && NULL != resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  bool result = auth_card_data->query.auth_card.result.verified;

  switch (core_status_get_flow_status()) {
    case MANAGER_AUTH_CARD_STATUS_SERIAL_SIGNED:
      if (false == result) {
        resp->which_response = MANAGER_AUTH_CARD_RESPONSE_FLOW_COMPLETE_TAG;
        resp->flow_complete.dummy_field = 0;
        delay_scr_init(ui_text_card_authentication_failed, DELAY_TIME);
        return MANAGER_TASK_SUCCESS;
      } else {
        return MANAGER_TASK_INVALID_STATE;
      }
      break;

    case MANAGER_AUTH_CARD_STATUS_CHALLENGE_SIGNED:
      if (false == result) {
        resp->which_response = MANAGER_AUTH_CARD_RESPONSE_FLOW_COMPLETE_TAG;
        resp->flow_complete.dummy_field = 0;
        delay_scr_init(ui_text_card_authentication_failed, DELAY_TIME);
        return MANAGER_TASK_SUCCESS;
      } else {
        if (true == auth_card_data->ctx.pair_card_required) {
          /* TODO: Handle this card pairing task*/

          /* TODO: Check if card operation failed*/
          // return MANAGER_TASK_FAILED;
        }

        core_status_set_flow_status(MANAGER_AUTH_CARD_STATUS_PAIRING_DONE);
        resp->which_response = MANAGER_AUTH_CARD_RESPONSE_FLOW_COMPLETE_TAG;
        resp->flow_complete.dummy_field = 0;
        delay_scr_init(ui_text_card_authentication_success, DELAY_TIME);

        return MANAGER_TASK_SUCCESS;
      }
      break;

    default:
      return MANAGER_TASK_INVALID_STATE;
      break;
  }

  return MANAGER_TASK_INVALID_DEFAULT;
}

manager_error_code_t handle_auth_card_query(
    auth_card_data_t *auth_card_data,
    manager_auth_card_response_t *resp) {
  if (NULL == auth_card_data || NULL == resp) {
    return MANAGER_TASK_INVALID_ARGS;
  }

  switch (auth_card_data->query.auth_card.which_request) {
    case MANAGER_AUTH_CARD_REQUEST_INITIATE_TAG:
      return handle_auth_card_initiate_query(auth_card_data, resp);
      break;

    case MANAGER_AUTH_CARD_REQUEST_CHALLENGE_TAG:
      return handle_auth_card_challenge_query(auth_card_data, resp);
      break;

    case MANAGER_AUTH_CARD_REQUEST_RESULT_TAG:
      return handle_auth_card_result_query(auth_card_data, resp);
      break;

    default:
      return MANAGER_TASK_UNKNOWN_QUERY_REQUEST;
      break;
  }

  return MANAGER_TASK_INVALID_DEFAULT;
}

/*****************************************************************************
 * GLOBAL FUNCTIONS
 *****************************************************************************/
void card_auth_handler(manager_query_t *query) {
  ASSERT(NULL != query);

  if (MANAGER_AUTH_CARD_REQUEST_INITIATE_TAG !=
      query->auth_card.which_request) {
    // Ignore invalid request
    return;
  }

  evt_status_t evt_status = {0};
  manager_auth_card_response_t resp = MANAGER_AUTH_CARD_RESPONSE_INIT_ZERO;
  auth_card_data_t auth_card_data = {
      .ctx =
          {
              .acceptable_cards = ACCEPTABLE_CARDS_ALL,
              .heading = "",
              .message = "",
              .pair_card_required = false,
          },
      .query = *query};

  core_status_set_flow_status(MANAGER_AUTH_CARD_STATUS_INIT);
  do {
    /* TODO: Create a handler to resolve specific returns and either send error
     * response or abort flow accordingly*/
    if (MANAGER_TASK_SUCCESS !=
        handle_auth_card_query(&auth_card_data, &resp)) {
      usb_clear_event();
      return;
    }

    if (resp.which_response != 0) {
      send_auth_card_response(&resp);
      memzero(&resp, sizeof(resp));
    }

    if (MANAGER_AUTH_CARD_STATUS_PAIRING_DONE ==
        core_status_get_flow_status()) {
      return;
    }

    evt_status = get_events(EVENT_CONFIG_USB, MAX_INACTIVITY_TIMEOUT);

    if (MANAGER_TASK_SUCCESS !=
        decode_and_verify_auth_card_query_if_usb_evt(&(evt_status.usb_event),
                                                     &(auth_card_data.query))) {
      usb_clear_event();
      return;
    }

  } while (false == evt_status.p0_event.flag ||
           MANAGER_AUTH_CARD_STATUS_PAIRING_DONE !=
               core_status_get_flow_status());

  HANDLE_P0_EVENTS();
}
