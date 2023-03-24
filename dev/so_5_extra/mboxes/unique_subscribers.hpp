/*!
 * \file
 * \brief Implementation of unique_subscribers mbox.
 *
 * \since
 * v.1.5.0
 */

#pragma once

#include <so_5/version.hpp>

#if SO_5_VERSION < SO_5_VERSION_MAKE(5u, 8u, 0u)
#error "SObjectizer-5.8.0 of newest is required"
#endif

#include <so_5/unique_subscribers_mbox.hpp>

#include <so_5_extra/error_ranges.hpp>

#include <memory>
#include <tuple>
#include <utility>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace unique_subscribers {

namespace errors {

/*!
 * \brief An attempt to make another subscription to the same message type.
 *
 * This error is reported when there is an existing subscription to
 * the same message type. And this subscription is made for another agent.
 *
 * \deprecated Since v.1.6.0 there is no such error code.
 *
 * \since
 * v.1.5.0
 */
const int rc_subscription_exists =
		so_5::extra::errors::mboxes_unique_subscribers_errors;

/*!
 * \brief An attempt to set a delivery filter.
 *
 * \deprecated Since v.1.5.1 delivery filters are supported by
 * unique_subscribers mbox.
 *
 * \since v.1.5.0
 */
[[deprecated]]
const int rc_delivery_filters_not_supported =
		so_5::extra::errors::mboxes_unique_subscribers_errors + 1;

} /* namespace errors */

//
// make_mbox
//
/*!
 * \brief Factory function for creation of a new instance of unique_subscribers
 * mbox.
 *
 * Usage examples:
 *
 * Create a mbox with std::mutex as Lock_Type (this mbox can safely be
 * used in multi-threaded environments):
 * \code
 * so_5::environment_t & env = ...;
 * auto mbox = so_5::extra::mboxes::unique_subscribers::make_mbox(env);
 * \endcode
 *
 * Create a mbox with so_5::null_mutex_t as Lock_Type (this mbox can only
 * be used in single-threaded environments):
 * \code
 * so_5::environment_t & env = ...;
 * auto mbox = so_5::extra::mboxes::unique_subscribers::make_mbox<so_5::null_mutex_t>(env);
 * \endcode
 *
 * \tparam Lock_Type type of lock to be used for thread safety. It can be
 * std::mutex or so_5::null_mutex_t (or any other type which can be used
 * with std::lock_quard).
 *
 * \deprecated Since v.1.6.0 it's just a synonum for so_5::make_unique_subscribers_mbox().
 * This function is kept here for compatibility reasons. It will be removed in
 * some future release of so5extra.
 *
 * \since v.1.5.0
 */
template<
	typename Lock_Type = std::mutex >
[[nodiscard]][[deprecated("Use so_5::make_unique_subscribers_mbox() instead")]]
mbox_t
make_mbox( so_5::environment_t & env )
	{
		return so_5::make_unique_subscribers_mbox< Lock_Type >( env );
	}

} /* namespace unique_subscribers */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

