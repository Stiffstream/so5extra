/*
 * An example of using collecting_mbox.
 *
 * There are three agents of type shard_t. They store various parts of
 * books descriptions (one agent stores only authors, another store only
 * titles, ...).
 *
 * Agents of type shard_t react on two message:
 * - store_book_t is send when it is necessary to store another book.
 *   Parts of book's description is stored by every agents and every
 *   agent send store_book_ack_t acknowledgement back;
 * - request_data_t is send when it is necessary to request all data
 *   related to a specific book. Every shard-agent sends back a message
 *   of type data_t.
 *
 * There is a agent of type sample_performer_t. It issues several
 * store_book_t messages. When it receives acks from all shard-agents
 * it issues request_data_t for a particular book.
 *
 * To receive acks and book's data agent sample_performer uses
 * collecting_mboxes. It means that sample_performer receives ack for
 * a particular store_book_t only when all shard-agents send its own
 * store_book_ack_t messages.
 */

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

// Which field from book's description will be stored by shared-agent.
enum class field_id_t
{
	author,
	title,
	summary
};

// Type for book's description.
struct book_description_t
{
	std::string m_author;
	std::string m_title;
	std::string m_summary;
};

// Helpers for extraction of fields from book's description.
template<field_id_t Field>
std::string get(const book_description_t &);

template<>
std::string get<field_id_t::author>(const book_description_t & b)
{
	return b.m_author;
}

template<>
std::string get<field_id_t::title>(const book_description_t & b)
{
	return b.m_title;
}

template<>
std::string get<field_id_t::summary>(const book_description_t & b)
{
	return b.m_summary;
}

// This message will be sent when description of a new book must be stored.
struct store_book_t : public so_5::message_t
{
	// Unique book ID.
	const int m_key;
	// Description of a new book.
	const book_description_t m_book;
	// Mbox for store_book_ack_t message.
	const so_5::mbox_t m_ack_to;

	store_book_t(int key, book_description_t book, so_5::mbox_t ack_to)
		:	m_key(key), m_book(std::move(book)), m_ack_to(std::move(ack_to))
	{}
};

// This message will be sent as acknowledgement for store_book_t.
struct store_book_ack_t : public so_5::message_t
{
	// Unique book ID.
	const int m_key;

	store_book_ack_t(int key)
		:	m_key(key)
	{}
};

// This message will be send when someone wants to get book's description.
struct request_data_t : public so_5::message_t
{
	// Unique book ID.
	const int m_key;
	// Mbox for data_t message.
	const so_5::mbox_t m_reply_to;

	request_data_t(int key, so_5::mbox_t reply_to)
		:	m_key(key), m_reply_to(std::move(reply_to))
	{}
};

// This message will be sent by shard-agents as reply to request_data_t message.
struct data_t : public so_5::message_t
{
	const int m_key;
	const field_id_t m_field;
	const std::string m_data;

	data_t(int key, field_id_t field, std::string data)
		:	m_key(key), m_field(field), m_data(std::move(data))
	{}
};

// Type of shard-agent.
template<field_id_t Field>
class shard_t final : public so_5::agent_t
{
public:
	shard_t(context_t ctx, so_5::mbox_t command_mbox)
		:	so_5::agent_t(std::move(ctx))
	{
		so_subscribe(command_mbox)
			.event(&shard_t::on_store_book)
			.event(&shard_t::on_request_data);
	}

private:
	using map_t = std::map<int, std::string>;

	map_t m_data;

	void on_store_book(mhood_t<store_book_t> cmd)
	{
		m_data[cmd->m_key] = get<Field>(cmd->m_book);
		so_5::send<store_book_ack_t>(cmd->m_ack_to, cmd->m_key);
	}

	void on_request_data(mhood_t<request_data_t> cmd)
	{
		so_5::send<data_t>(cmd->m_reply_to, cmd->m_key, Field, m_data[cmd->m_key]);
	}
};

// Type of example performer agent.
class sample_performer_t final : public so_5::agent_t
{
	// Count of books is known at the compile-time.
	static constexpr std::size_t total_books = 3;

public:
	sample_performer_t(context_t ctx, so_5::mbox_t command_mbox)
		:	so_5::agent_t(std::move(ctx))
		,	m_command_mbox(std::move(command_mbox))
	{
		so_subscribe_self()
			.event(&sample_performer_t::on_store_ack)
			.event(&sample_performer_t::on_data);
	}

	virtual void so_evt_start() override
	{
		// Store all example books at the start of work.
		std::array<book_description_t, total_books> books{{
			{ "Miguel De Cervantes", "Don Quixote",
				"The story of the gentle knight and his servant Sancho Panza has "
				"entranced readers for centuries. " },
			{ "Jonathan Swift", "Gulliver's Travels",
				"A wonderful satire that still works for all ages, despite the "
				"savagery of Swift's vision." },
			{ "Stendhal", "The Charterhouse of Parma",
				"Penetrating and compelling chronicle of life in an Italian "
				"court in post-Napoleonic France." }
		}};

		int key = 0;
		for( const auto & b : books )
		{
			so_5::send<store_book_t>(m_command_mbox,
					key,
					b,
					// Create new collecting mbox for every book.
					// This prevents from collection of acks for different books
					// in one messages_collected instance.
					store_ack_mbox_t::make(so_direct_mbox()));

			++key;
		}
	}

private:
	// Type of collecting mbox for store_book_ack_t messages.
	using store_ack_mbox_t = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
			store_book_ack_t,
			so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t<3>>;

	// Type of collecting mbox for data_t messages.
	using data_mbox_t = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
			data_t,
			so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t<3>>;

	// Mbox to be used for communication with shard-agents.
	const so_5::mbox_t m_command_mbox;
	// Count of processed books.
	// It is necessary for shutdown of example.
	std::size_t m_books_received = 0;

	void on_store_ack(mhood_t<store_ack_mbox_t::messages_collected_t> cmd)
	{
		// Key will be same for every message.
		// So we extract it from the first message.
		const auto key = cmd->with_nth(0, [](auto m) { return m->m_key; });
		std::cout << "Book with key=" << key << " is stored" << std::endl;

		// Initiate request_data_t for this book.
		so_5::send<request_data_t>(m_command_mbox,
				key,
				// Create a new collecting mbox for this request.
				data_mbox_t::make(so_direct_mbox()));
	}

	void on_data(mhood_t<data_mbox_t::messages_collected_t> cmd)
	{
		// Key will be same for every message.
		// So we extract it from the first message.
		const auto key = cmd->with_nth(0, [](auto m) { return m->m_key; });

		// A full book's description must be constructed from different parts.
		// Use for_each to handle every collected message.
		book_description_t book;
		cmd->for_each([&book](auto m) {
			if(field_id_t::author == m->m_field)
				book.m_author = m->m_data;
			else if(field_id_t::title == m->m_field)
				book.m_title = m->m_data;
			else if(field_id_t::summary == m->m_field)
				book.m_summary = m->m_data;
		});

		std::cout << "Book with key=" << key << " is {"
				<< book.m_author << ", '" << book.m_title << "', "
				<< book.m_summary << "}" << std::endl;

		++m_books_received;
		if(total_books == m_books_received)
			so_deregister_agent_coop_normally();
	}
};

void init(so_5::environment_t & env)
{
	// All example's agents will work in one coop.
	env.introduce_coop([&](so_5::coop_t & coop) {
		// Shard-agents will live on separate work threads.
		auto disp = so_5::disp::active_obj::make_dispatcher(env);

		auto command_mbox = env.create_mbox();

		coop.make_agent_with_binder<shard_t<field_id_t::author>>(
				disp.binder(),
				command_mbox);
		coop.make_agent_with_binder<shard_t<field_id_t::title>>(
				disp.binder(),
				command_mbox);
		coop.make_agent_with_binder<shard_t<field_id_t::summary>>(
				disp.binder(),
				command_mbox);

		// Example performer will work on the default dispatcher.
		coop.make_agent<sample_performer_t>(command_mbox);
	});
}

int main()
{
	try
	{
		so_5::launch( &init );
	}
	catch(const std::exception & x)
	{
		std::cerr << "Exception caught: " << x.what() << std::endl;
	}
}

