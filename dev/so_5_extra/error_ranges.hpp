/*!
 * \brief Ranges for error codes of each submodules.
 *
 * \since
 * v.1.0.2
 */
#pragma once

namespace so_5 {

namespace extra {

namespace errors {

//! Starting point for errors of shutdowner submodule.
const int shutdowner_errors = 20000;

//! Starting point for errors of collecting_mbox submodule.
const int collecting_mbox_errors = 20100;

//! Starting point for errors of asio_thread_pool submodule.
const int asio_thread_pool_errors = 20200;

//! Starting point for errors of retained_msg_mbox submodule.
/*!
 * \since
 * v.1.0.3
 */
const int retained_msg_mbox_errors = 20300;

//! Starting point for errors of async_op submodule.
/*!
 * \since
 * v.1.0.4
 */
const int async_op_errors = 20400;

//! Starting point for errors of mboxes::proxy submodule.
/*!
 * \since
 * v.1.2.0
 */
const int mboxes_proxy_errors = 20500;

//! Starting point for errors of revocable_timer submodule.
/*!
 * \since
 * v.1.2.0
 */
const int revocable_timer_errors = 20600;

//! Starting point for errors of enveloped_msg submodule.
/*!
 * \since
 * v.1.2.0
 */
const int enveloped_msg_errors = 20700;

//! Starting point for errors of revocable_msg submodule.
/*!
 * \since
 * v.1.2.0
 */
const int revocable_msg_errors = 20800;

//! Starting point for errors of sync submodule.
/*!
 * \since
 * v.1.3.0
 */
const int sync_errors = 20900;

//! Starting point for errors of asio_one_thread submodule.
/*!
 * \since
 * v.1.4.1
 */
const int asio_one_thread_errors = 21000;

} /* namespace errors */

} /* namespace extra */

} /* namespace so_5 */

