/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 NKI/AVL, Netherlands Cancer Institute
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

#include "dssp.hpp"
#include "databank-service.hpp"
#include "db-connection.hpp"

#include "revision.hpp"

#include <cif++.hpp>

#include <gxrio.hpp>
#include <mcfp.hpp>

#include <zeep/http/daemon.hpp>
#include <zeep/http/html-controller.hpp>
#include <zeep/http/rest-controller.hpp>
#include <zeep/streambuf.hpp>

namespace fs = std::filesystem;

// --------------------------------------------------------------------

class dssp_html_controller : public zeep::http::html_controller
{
  public:
	dssp_html_controller()
		: zeep::http::html_controller()
	{
		mount("{css,scripts,fonts,images,favicon}/", &dssp_html_controller::handle_file);
		mount("{favicon.ico,browserconfig.xml,manifest.json}", &dssp_html_controller::handle_file);
		map_get("", "index");
		map_get("about", "about");
		map_get("download", "download");
		map_get("license", "license");
		
		map_get("get", &dssp_html_controller::get, "pdb-id", "format");

		map_get("db/{pdb-id}", &dssp_html_controller::db_mmcif, "pdb-id");
		map_get("db/{pdb-id}/mmcif", &dssp_html_controller::db_mmcif, "pdb-id");
		map_get("db/{pdb-id}/legacy", &dssp_html_controller::db_legacy, "pdb-id");
	}

	zeep::http::reply get(const zeep::http::scope& scope, std::string pdb_id, std::optional<std::string> format);

	zeep::http::reply db_mmcif(const zeep::http::scope& scope, std::string pdb_id);
	zeep::http::reply db_legacy(const zeep::http::scope& scope, std::string pdb_id);
};

zeep::http::reply dssp_html_controller::db_mmcif(const zeep::http::scope& scope, std::string pdb_id)
{
	zeep::to_lower(pdb_id);

	auto file = databank_service::instance().get(pdb_id, "mmcif");

	if (not fs::exists(file))
	{
		zeep::http::scope sub(scope);

		sub.put("pdb-id", pdb_id);
		auto reply = get_template_processor().create_reply_from_template("not-found", sub);

		reply.set_status(zeep::http::not_found);
		return reply;
	}

	zeep::http::reply rep(zeep::http::ok, {1, 1});

	if (file.extension() != ".gz")
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
	else if (get_header("accept-encoding").find("gzip") != std::string::npos)
	{
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
		rep.set_header("content-encoding", "gzip");
	}
	else
	{
		cif::gzio::ifstream in(file);

		if (not in.is_open())
			return zeep::http::reply(zeep::http::not_found, {1, 1});
		
		std::stringstream os;
		os << in.rdbuf();

		rep.set_content(os.str(), "text/plain");

		if (rep.get_content().empty())
			throw std::runtime_error("Databank file is corrupt");
	}

	std::string filename = pdb_id + ".cif";
	rep.set_header("content-disposition", "attachement; filename = \"" + filename + '\"');

	return rep;
}

zeep::http::reply dssp_html_controller::db_legacy(const zeep::http::scope& scope, std::string pdb_id)
{
	zeep::to_lower(pdb_id);

	auto file = databank_service::instance().get(pdb_id, "dssp");

	if (not fs::exists(file))
	{
		zeep::http::scope sub(scope);

		sub.put("pdb-id", pdb_id);
		auto reply = get_template_processor().create_reply_from_template("not-found", sub);

		reply.set_status(zeep::http::not_found);
		return reply;
	}

	zeep::http::reply rep(zeep::http::ok, {1, 1});

	if (file.extension() != ".gz")
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
	else if (get_header("accept-encoding").find("gzip") != std::string::npos)
	{
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
		rep.set_header("content-encoding", "gzip");
	}
	else
	{
		cif::gzio::ifstream in(file);

		if (not in.is_open())
			return zeep::http::reply(zeep::http::not_found, {1, 1});
		
		std::stringstream os;
		os << in.rdbuf();

		rep.set_content(os.str(), "text/plain");

		if (rep.get_content().empty())
			throw std::runtime_error("Databank file is corrupt");
	}

	std::string filename = pdb_id + ".dssp";
	rep.set_header("content-disposition", "attachement; filename = \"" + filename + '\"');

	return rep;
}

zeep::http::reply dssp_html_controller::get(const zeep::http::scope& scope, std::string pdb_id, std::optional<std::string> format)
{
	zeep::to_lower(pdb_id);

	auto file = databank_service::instance().get(pdb_id, format.value_or("mmcif"));

	if (not fs::exists(file))
	{
		zeep::http::scope sub(scope);

		sub.put("pdb-id", pdb_id);
		auto reply = get_template_processor().create_reply_from_template("not-found", sub);

		reply.set_status(zeep::http::not_found);
		return reply;
	}

	zeep::http::reply rep(zeep::http::ok, {1, 1});

	if (file.extension() != ".gz")
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
	else if (get_header("accept-encoding").find("gzip") != std::string::npos)
	{
		rep.set_content(new std::ifstream(file, std::ios::binary), "text/plain");
		rep.set_header("content-encoding", "gzip");
	}
	else
	{
		cif::gzio::ifstream in(file);

		if (not in.is_open())
			return zeep::http::reply(zeep::http::not_found, {1, 1});
		
		std::stringstream os;
		os << in.rdbuf();

		rep.set_content(os.str(), "text/plain");

		if (rep.get_content().empty())
			throw std::runtime_error("Databank file is corrupt");
	}

	std::string filename = pdb_id + (format == "dssp" ? ".dssp" : ".cif");
	rep.set_header("content-disposition", "attachement; filename = \"" + filename + '\"');

	return rep;
}

// --------------------------------------------------------------------

class dssp_rest_controller : public zeep::http::rest_controller
{
  public:
	dssp_rest_controller()
		: zeep::http::rest_controller("")
	{
		map_post_request("do", &dssp_rest_controller::work, "data", "format");
	}

	zeep::http::reply work(const zeep::http::file_param &coordinates, std::optional<std::string> format);
};

zeep::http::reply dssp_rest_controller::work(const zeep::http::file_param &coordinates, std::optional<std::string> format)
{
	zeep::char_streambuf sb(coordinates.data, coordinates.length);

	gxrio::istream in(&sb);

	cif::file f = cif::pdb::read(in);
	if (f.empty())
		throw std::runtime_error("Invalid input file, is it empty?");

	// --------------------------------------------------------------------

	short pp_stretch = 3; //minPPStretch.value_or(3);

	std::string fmt = format.value_or("mmcif");

	dssp dssp(f.front(), 1, pp_stretch, true);

	std::ostringstream os;

	if (fmt == "dssp")
		dssp.write_legacy_output(os);
	else
	{
		dssp.annotate(f.front(), true, true);
		os << f.front();
	}

	// --------------------------------------------------------------------
	
	std::string name = f.front().name();
	if (fmt == "dssp")
		name += ".dssp";
	else
		name += ".cif";

	zeep::http::reply rep{ zeep::http::ok };
	rep.set_content(os.str(), "text/plain");
	rep.set_header("content-disposition", "attachement; filename = \"" + name + '"');

	return rep;
}

// --------------------------------------------------------------------

int main(int argc, char *argv[])
{
	using namespace std::literals;
	namespace zh = zeep::http;

	cif::compound_factory::init(true);

	int result = 0;

	auto &config = mcfp::config::instance();

	config.init("dsspd [options] start|stop|status|reload",
		mcfp::make_option("help,h", "Display help message"),
		mcfp::make_option("version", "Print version"),
		mcfp::make_option("verbose,v", "verbose output"),

		mcfp::make_option<std::string>("address", "0.0.0.0", "External address"),
		mcfp::make_option<uint16_t>("port", 10351, "Port to listen to"),
		mcfp::make_option<std::string>("user,u", "www-data", "User to run the daemon"),
		mcfp::make_option<std::string>("context", "", "Root context for web server"),

		mcfp::make_option("no-daemon,F", "Do not fork into background"),

		mcfp::make_option<std::string>("pdb-dir", "Directory containing the PDB mmCIF files"),
		mcfp::make_option<std::string>("dssp-dir", "Directory containing the DSSP databank files"),
		mcfp::make_option<std::string>("legacy-dssp-dir", "Directory containing the DSSP databank files in legacy format"),
		mcfp::make_option<unsigned>("update-threads", 1, "Number of update threads to run simultaneously"),

		mcfp::make_option<std::string>("db-dbname", "dssp-db name"),
		mcfp::make_option<std::string>("db-user", "dssp-db owner"),
		mcfp::make_option<std::string>("db-password", "dssp-db password"),
		mcfp::make_option<std::string>("db-host", "dssp-db host"),
		mcfp::make_option<std::string>("db-port", "dssp-db port"),

		mcfp::make_option<std::string>("config", "Config file to use"));

	std::error_code ec;

	config.parse(argc, argv, ec);
	if (ec)
	{
		std::cerr << "Error parsing command line arguments: " << ec.message() << std::endl
				  << std::endl
				  << config << std::endl;
		exit(1);
	}

	config.parse_config_file("config", "dsspd.conf", { ".", "/etc" }, ec);
	if (ec)
	{
		std::cerr << "Error parsing config file: " << ec.message() << std::endl;
		exit(1);
	}

	// --------------------------------------------------------------------

	if (config.has("version"))
	{
		write_version_string(std::cout, config.has("verbose"));
		exit(0);
	}

	if (config.has("help"))
	{
		std::cout << config << std::endl;
		exit(0);
	}

	if (config.operands().empty())
	{
		std::cerr << "Missing command, should be one of start, stop, status or reload" << std::endl;
		exit(1);
	}

	cif::VERBOSE = config.count("verbose");

	// --------------------------------------------------------------------

	std::string user = config.get<std::string>("user");
	std::string address = config.get<std::string>("address");
	uint16_t port = config.get<uint16_t>("port");

	zeep::http::daemon server([&, context = config.get("context")]()
	{
		db_connection::init();
		databank_service::instance();

		auto s = new zeep::http::server();

#ifndef NDEBUG
		s->set_template_processor(new zeep::http::file_based_html_template_processor("docroot"));
#else
		s->set_template_processor(new zeep::http::rsrc_based_html_template_processor());
#endif
		s->add_controller(new dssp_rest_controller());
		s->add_controller(new dssp_html_controller());

		s->set_context_name(context);

		return s;
	}, kProjectName);

	std::string command = config.operands().front();

	if (command == "start")
	{
		std::cout << "starting server at http://" << address << ':' << port << '/' << std::endl;

		if (config.has("no-daemon"))
			result = server.run_foreground(address, port);
		else
			result = server.start(address, port, 1, 10, user);
	}
	else if (command == "stop")
		result = server.stop();
	else if (command == "status")
		result = server.status();
	else if (command == "reload")
		result = server.reload();
	else
	{
		std::cerr << "Invalid command" << std::endl;
		result = 1;
	}

	return result;
}