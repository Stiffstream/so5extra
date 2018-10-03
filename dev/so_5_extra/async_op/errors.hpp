/*!
 * \file
 * \brief Error codes for %async_op submodule.
 *
 * \since
 * v.1.0.4
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

namespace so_5 {

namespace extra {

namespace async_op {

namespace errors {

/*!
 * \brief An attempt to perform some action which can't be done
 * when async operation is activated.
 *
 * \since
 * v.1.0.4
 */
const int rc_async_op_activated =
		so_5::extra::errors::async_op_errors;

/*!
 * \brief An attempt to activate async operation without defined
 * completion handlers.
 *
 * \since
 * v.1.0.4
 */
const int rc_no_completion_handler =
		so_5::extra::errors::async_op_errors + 1;

/*!
 * \brief An attempt to perform some action on async operation which
 * can't be activated.
 *
 * \since
 * v.1.0.4
 */
const int rc_op_cant_be_activated =
		so_5::extra::errors::async_op_errors + 2;

/*!
 * \brief An attempt to use event handler for different message type.
 *
 * \since
 * v.1.0.4
 */
const int rc_msg_type_mismatch =
		so_5::extra::errors::async_op_errors + 3;

/*!
 * \brief An attempt to use empty definition_point object.
 *
 * For example an attempt to use definition_point object after
 * a call to activate.
 */
const int rc_empty_definition_point_object =
		so_5::extra::errors::async_op_errors + 4;

} /* namespace errors */

} /* namespace async_op */

} /* namespace extra */

} /* namespace so_5 */

