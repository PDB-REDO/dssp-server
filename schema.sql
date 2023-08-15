drop table if exists pdb_ref_seq_chain cascade;
drop table if exists pdb_ref_seq cascade;
drop table if exists pdb_entry cascade;

create table pdb_entry (
	id varchar primary key,
	file_date timestamp with time zone default CURRENT_TIMESTAMP not null
);

create table pdb_ref_seq (
	id serial primary key,
	pdb_id varchar not null references pdb_entry(id) on delete cascade deferrable initially deferred,
	entity_type varchar not null,
	entity_poly_type varchar not null,
	db_accession varchar not null,
	align_beg integer,
	align_end integer
);

create table pdb_ref_seq_chain (
	ref_seq_id bigint not null references pdb_ref_seq(id) on delete cascade deferrable initially deferred,
	chain_id varchar not null
);

create index pdb_db_ref_ix_1 on pdb_ref_seq(pdb_id);
create index pdb_db_ref_ix_3 on pdb_ref_seq(db_accession);

alter table pdb_ref_seq_chain owner to "dssp-admin";
alter table pdb_ref_seq owner to "dssp-admin";
alter table pdb_entry owner to "dssp-admin";
