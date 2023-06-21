/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2020 NKI/AVL, Netherlands Cancer Institute
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "db-connection.hpp"

#include <mcfp.hpp>

#include <date/date.h>

#include <iostream>
#include <regex>

// --------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> parse_timestamp(std::string timestamp)
{
	using namespace date;
	using namespace std::chrono;

	if (timestamp[10] == 'T')
		timestamp[10] = ' ';

	std::regex kRX(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?(Z|[-+]\d{2}(?::\d{2})?)?)");
	std::smatch m;

	if (not std::regex_match(timestamp, m, kRX))
		throw std::runtime_error("Invalid date format");

	std::istringstream is(timestamp);

	time_point<system_clock> result;

	if (m[1].matched)
	{
		if (m[1] == "Z")
			is >> parse("%F %TZ", result);
		else
			is >> parse("%F %T%0z", result);
	}
	else
		is >> parse("%F %T", result);

	if (is.bad() or is.fail())
		throw std::runtime_error("invalid formatted date");

	return result;
}

// --------------------------------------------------------------------

std::unique_ptr<db_connection> db_connection::s_instance;
thread_local std::unique_ptr<pqxx::connection> db_connection::s_connection;

void db_connection::init()
{
	std::ostringstream connection_string;
	auto &config = mcfp::config::instance();

	for (std::string opt : {"db-host", "db-port", "db-dbname", "db-user", "db-password"})
	{
		if (not config.has(opt))
			continue;

		connection_string << opt.substr(3) << "=" << config.get(opt) << ' ';
	}

	s_instance.reset(new db_connection(connection_string.str()));
}

db_connection& db_connection::instance()
{
	return *s_instance;
}

// --------------------------------------------------------------------

db_connection::db_connection(const std::string& connectionString)
	: m_connection_string(connectionString)
{
}

pqxx::connection& db_connection::get_connection()
{
	if (not s_connection)
		s_connection.reset(new pqxx::connection(m_connection_string));
	return *s_connection;
}

void db_connection::reset()
{
	s_connection.reset();
}

// --------------------------------------------------------------------

bool db_error_handler::create_error_reply(const zeep::http::request& req, std::exception_ptr eptr, zeep::http::reply& reply)
{
	try
	{
		std::rethrow_exception(eptr);
	}
	catch (pqxx::broken_connection& ex)
	{
		std::cerr << ex.what() << std::endl;
		db_connection::instance().reset();
	}
	catch (...)
	{
	}
	
	return false;
}
