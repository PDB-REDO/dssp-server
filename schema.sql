drop table if exists pdb_db_ref cascade;
drop table if exists pdb_file cascade;

create table pdb_file (
	id varchar primary key,
	file_date timestamp with time zone default CURRENT_TIMESTAMP not null
);

create table pdb_db_ref (
	id serial primary key,
	pdb_id varchar not null references pdb_file(id) on delete cascade deferrable initially deferred,
	db_code varchar not null,
	db_name varchar not null,
	db_accession varchar not null
);

create index pdb_db_ref_ix_1 on pdb_db_ref(pdb_id);
create index pdb_db_ref_ix_2 on pdb_db_ref(db_name);
create index pdb_db_ref_ix_3 on pdb_db_ref(db_accession);

alter table pdb_db_ref owner to "dssp-admin";
alter table pdb_file owner to "dssp-admin";
