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

#pragma once

#include <zeep/nvp.hpp>

#include <condition_variable>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>

struct uniprot_info
{
	std::string id, acc;
	uint32_t start, end;

	template <typename Archive>
	void serialize(Archive &ar, unsigned long version)
	{
		ar & zeep::make_nvp("id", id)
		   & zeep::make_nvp("acc", acc)
		   & zeep::make_nvp("start", start)
		   & zeep::make_nvp("end", end);
	}
};

struct pdb_entry
{
	std::string id;
	std::chrono::time_point<std::chrono::system_clock> file_date;
	uniprot_info unp;

	template <typename Archive>
	void serialize(Archive &ar, unsigned long version)
	{
		ar & zeep::make_nvp("id", id)
		   & zeep::make_nvp("created", file_date)
		   & zeep::make_nvp("unp", unp);
	}
};

class databank_service
{
  public:
	static databank_service &instance();

	~databank_service();

	void submit_db_request(const std::string &pdb_id);

	void check_ref_info(const std::string &pdb_id) const;

	std::filesystem::path get(const std::string &pdb_id, const std::string &format)
	{
		if (format == "dssp")
			return get_legacy_dssp_file_for_pdb_id(pdb_id);
		else if (format == "mmcif")
			return get_dssp_file_for_pdb_id(pdb_id);
		else
			return {};
	}

	std::vector<pdb_entry> get_entries_for_code_or_acc(const std::string &acc) const;

  private:
	databank_service();

	void run();
	void scan();

	bool needs_update(const std::string &pdb_id) const;

	std::filesystem::path get_pdb_file_for_pdb_id(const std::string &pdb_id) const;
	std::filesystem::path get_dssp_file_for_pdb_id(const std::string &pdb_id) const;
	std::filesystem::path get_legacy_dssp_file_for_pdb_id(const std::string &pdb_id) const;

	std::filesystem::path m_pdb_dir;
	std::filesystem::path m_dssp_dir;
	std::filesystem::path m_legacy_dssp_dir;

	int m_inotify_fd;
	bool m_stop = false;

	struct entry
	{
		entry(const std::string &pdb_id, std::filesystem::file_time_type t)
			: pdb_id(pdb_id)
			, timestamp(t)
		{
		}

		entry() = default;
		entry(const entry &) = default;
		entry &operator=(const entry &) = default;

		std::string pdb_id;
		std::filesystem::file_time_type timestamp;

		bool operator<(const entry &rhs) const
		{
			return timestamp < rhs.timestamp;
		}
	};

	std::mutex m_mutex;
	std::priority_queue<entry> m_queue;
	std::vector<std::thread> m_threads;
	std::condition_variable m_cv;
	std::chrono::time_point<std::chrono::system_clock> m_last_scan = {};
};
