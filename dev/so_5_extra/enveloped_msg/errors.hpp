/*!
 * \file
 * \brief Error codes for enveloped_msg submodule.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

namespace so_5 {

namespace extra {

namespace enveloped_msg {

namespace errors {

//! Mutability of payload message inside just_envelope can't be changed.
/*!
 * An envelope implemented by just_envelope_t class used mutability
 * flag from the enveloped payload. This mutability flag can't be changed
 * after construction of envelope.
 *
 * \since
 * v.1.2.0
 */
const int rc_mutabilty_of_envelope_cannot_be_changed =
	::so_5::extra::errors::enveloped_msg_errors + 1;

//! An attempt to use empty payload_holder for make an envelope.
/*!
 * This error can be reported is the same payload_hodler will be used
 * more than one time. For example:
 * \code
 * namespace env_ns = so_5::extra::enveloped_msg;
 * auto payload = env_ns::make<my_message>(...);
 * payload.envelope<env_ns::just_envelope_t>().send_to(...);
 * // Now payload object is empty because its content was enveloped and sent.
 * // This call will lead to rc_empty_payload_holder error.
 * payload.envelope<another_envelope_type>().send_to(...);
 * \endcode
 * \since
 * v.1.2.0
 */
const int rc_empty_payload_holder =
	::so_5::extra::errors::enveloped_msg_errors + 2;

} /* namespace errors */

} /* namespace enveloped_msg */

} /* namespace extra */

} /* namespace so_5 */

