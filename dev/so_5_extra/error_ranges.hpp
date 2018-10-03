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

} /* namespace errors */

} /* namespace extra */

} /* namespace so_5 */

