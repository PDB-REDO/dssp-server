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

#include "databank-service.hpp"
#include "db-connection.hpp"
#include "dssp.hpp"

#include <cif++.hpp>

#include <mcfp.hpp>

#include <functional>
#include <iostream>
#include <queue>

namespace fs = std::filesystem;

databank_service &databank_service::instance()
{
	static databank_service s_instance;
	return s_instance;
}

databank_service::databank_service()
{
	auto &config = mcfp::config::instance();

	m_dssp_dir = config.get("dssp-dir");
	m_pdb_dir = config.get("pdb-dir");

	std::error_code ec;
	if (not fs::exists(m_dssp_dir, ec))
		fs::create_directories(m_dssp_dir, ec);

	if (ec)
	{
		std::cerr << "Failed to create DSSP databank directory " << m_dssp_dir << std::endl;
		exit(1);
	}

	if (config.has("legacy-dssp-dir"))
	{
		m_legacy_dssp_dir = config.get("legacy-dssp-dir");

		if (not fs::exists(m_legacy_dssp_dir, ec))
			fs::create_directories(m_legacy_dssp_dir, ec);

		if (ec)
		{
			std::cerr << "Failed to create DSSP databank directory " << m_legacy_dssp_dir << std::endl;
			exit(1);
		}
	}

	unsigned nrOfThreads = config.get<unsigned>("update-threads");

	for (unsigned i = 0; i < nrOfThreads; ++i)
		m_threads.emplace_back(std::bind(&databank_service::run, this));
}

databank_service::~databank_service()
{
	m_stop = true;
	m_cv.notify_all();

	for (auto &t : m_threads)
		t.join();
}

void databank_service::submit_db_request(const std::string &pdb_id)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	auto p = get_pdb_file_for_pdb_id(pdb_id);
	m_queue.emplace(pdb_id, fs::last_write_time(p));
	m_cv.notify_one();
}

void databank_service::run()
{
	using namespace std::literals;

	while (not m_stop)
	{
		entry next;

		{
			std::unique_lock<std::mutex> lock(m_mutex);

			if (m_stop)
				continue;

			if (m_queue.empty())
			{
				if ((std::chrono::system_clock::now() - m_last_scan) > 24h)
				{
					scan();
					m_last_scan = std::chrono::system_clock::now();
				}
				else
					m_cv.wait_for(lock, 1s);

				continue;
			}

			next = m_queue.top();
			m_queue.pop();
		}

		try
		{
			cif::file f(get_pdb_file_for_pdb_id(next.pdb_id));

			// See if the data will fit at all
			bool calculateSurface = true;
			auto &db = f.front();
			for (const auto &[chain_id, seq_nr] : db["pdbx_poly_seq_scheme"].rows<std::string, int>("pdb_strand_id", "pdb_seq_num"))
			{
				if (chain_id.length() > 1 or seq_nr > 99999)
				{
					calculateSurface = false;
					break;
				}
			}

			std::cerr << "processing " << next.pdb_id << std::endl;
			dssp dssp(f.front(), 1, 3, calculateSurface);

			auto dssp_file = get_dssp_file_for_pdb_id(next.pdb_id);
			std::error_code ec;

			if (not fs::exists(dssp_file.parent_path(), ec))
				fs::create_directories(dssp_file.parent_path(), ec);

			if (ec)
				throw std::runtime_error("Could not create directory for DSSP file " + dssp_file.string());

			auto dssp_tmp_file = dssp_file.parent_path() / (dssp_file.filename().string() + ".tmp");

			cif::gzio::ofstream outCif(dssp_tmp_file);

			if (not outCif.is_open())
				throw std::runtime_error("Could not create DSSP file " + dssp_tmp_file.string());

			try
			{
				dssp.annotate(f.front(), true, true);
				outCif << f.front();

				outCif.close();

				fs::remove(dssp_file, ec);
				if (ec)
					throw std::runtime_error("Could not remove old DSSP databank file " + dssp_file.string());

				fs::rename(dssp_tmp_file, dssp_file, ec);

				if (ec)
					throw std::runtime_error("Could not rename old DSSP databank temp file");
			}
			catch (const std::exception &ex)
			{
				std::cerr << "Error creating " << dssp_file.string() << ": " << ex.what() << std::endl;
				fs::remove(dssp_file, ec);
			}

			// legacy file?
			auto legacy_dssp_file = get_legacy_dssp_file_for_pdb_id(next.pdb_id);
			if (not calculateSurface or legacy_dssp_file.empty())
				continue;

			if (not fs::exists(legacy_dssp_file.parent_path(), ec))
				fs::create_directories(legacy_dssp_file.parent_path(), ec);

			if (ec)
				throw std::runtime_error("Could not create directory for DSSP file " + legacy_dssp_file.string());

			auto legacy_dssp_tmp_file = legacy_dssp_file.parent_path() / (legacy_dssp_file.filename().string() + ".tmp");

			std::ofstream outDssp(legacy_dssp_tmp_file);

			if (not outDssp.is_open())
				throw std::runtime_error("Could not create DSSP file " + legacy_dssp_tmp_file.string());

			try
			{
				dssp.write_legacy_output(outDssp);
				outDssp.close();

				fs::remove(legacy_dssp_file, ec);
				if (ec)
					throw std::runtime_error("Could not remove old DSSP databank file " + legacy_dssp_file.string());

				fs::rename(legacy_dssp_tmp_file, legacy_dssp_file, ec);

				if (ec)
					throw std::runtime_error("Could not rename old DSSP databank temp file");
			}
			catch (const std::exception &ex)
			{
				std::cerr << "Error creating " << legacy_dssp_file.string() << ": " << ex.what() << std::endl;
				fs::remove(dssp_file, ec);
			}
			
		}
		catch (const std::exception &ex)
		{
			std::cerr << ex.what() << std::endl;
		}
	}
}

void databank_service::scan()
{
	for (auto di = fs::recursive_directory_iterator(m_pdb_dir); di != fs::recursive_directory_iterator(); ++di)
	{
		if (not di->is_regular_file())
			continue;

		auto p = fs::canonical(di->path());
		auto n = p.filename();

		if (n.has_extension() and n.extension() == ".gz")
			n.replace_extension();

		if (not n.has_extension() or n.extension() != ".cif")
			continue;

		n.replace_extension();

		check_ref_info(n.string());

		// So n should now be the pdb_id, right?
		if (not needs_update(n.string()))
			continue;

		m_queue.emplace(n, fs::last_write_time(p));
	}
}

fs::path databank_service::get_pdb_file_for_pdb_id(const std::string &pdb_id) const
{
	return m_pdb_dir / pdb_id.substr(1, 2) / (pdb_id + ".cif.gz");
}

fs::path databank_service::get_dssp_file_for_pdb_id(const std::string &pdb_id) const
{
	return m_dssp_dir / pdb_id.substr(1, 2) / (pdb_id + ".cif.gz");
}

fs::path databank_service::get_legacy_dssp_file_for_pdb_id(const std::string &pdb_id) const
{
	return m_legacy_dssp_dir.empty() ? fs::path{} : m_legacy_dssp_dir / pdb_id.substr(1, 2) / (pdb_id + ".dssp");
}

bool databank_service::needs_update(const std::string &pdb_id) const
{
	auto pdb_file = get_pdb_file_for_pdb_id(pdb_id);
	auto dssp_file = get_dssp_file_for_pdb_id(pdb_id);

	return fs::exists(pdb_file) and
	       (not fs::exists(dssp_file) or fs::last_write_time(pdb_file) > fs::last_write_time(dssp_file));
}

// --------------------------------------------------------------------

void databank_service::check_ref_info(const std::string &pdb_id) const
{
	using namespace std::chrono;
	using namespace std::literals;

	auto pdb_file = get_pdb_file_for_pdb_id(pdb_id);
	if (not fs::exists(pdb_file))
		return;

	auto ft = fs::last_write_time(pdb_file);
	auto scft = time_point_cast<system_clock::duration>(ft - decltype(ft)::clock::now() + system_clock::now());

	pqxx::transaction tx(db_connection::instance());
	auto r = tx.exec(R"(SELECT trim(both '"' from to_json(file_date)::text) AS file_date FROM pdb_file WHERE id = )" + tx.quote(pdb_id));

	bool needsUpdate = r.empty();
	if (not needsUpdate)
	{
		auto file_date = parse_timestamp(r.front()[0].as<std::string>());
		needsUpdate = (scft - file_date) > 24h;
	}

	if (needsUpdate)
	{
		using namespace cif::literals;

		cif::file f(pdb_file);
		auto &db = f.front();

		tx.exec(R"(DELETE FROM pdb_file WHERE id = )" + tx.quote(pdb_id));

		auto v_t = std::chrono::system_clock::to_time_t(scft);
		std::ostringstream ss;
		ss << std::put_time(std::localtime(&v_t), "[%FT%T%z]");

		tx.exec(R"(INSERT INTO pdb_file (id, file_date) VALUES ()" + tx.quote(pdb_id) + ", " + tx.quote(ss.str()) + ")");

		for (const auto &[db_code, db_name, acc] : db["struct_ref"].rows<std::string,std::string,std::string>("db_code", "db_name", "pdbx_db_accession"))
		{
			tx.exec0(
				R"(INSERT INTO pdb_db_ref (pdb_id, db_code, db_name, db_accession)
				VALUES ()" + tx.quote(pdb_id) + ", " + tx.quote(db_code) + ", " + tx.quote(db_name) + ", " + tx.quote(acc) + R"())");
		}
	}

	tx.commit();
}