--
-- PostgreSQL database dump
--

\restrict GzjWoLaKJbP8jb1ga0WXeZbYFwO3rdxhlh1hdJP35twZOPYAVG2fNdpeg5T1s83

-- Dumped from database version 16.13 (Ubuntu 16.13-0ubuntu0.24.04.1)
-- Dumped by pg_dump version 16.13 (Ubuntu 16.13-0ubuntu0.24.04.1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: public; Type: SCHEMA; Schema: -; Owner: postgres
--

-- *not* creating schema, since initdb creates it


ALTER SCHEMA public OWNER TO postgres;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: pdb_db_ref; Type: TABLE; Schema: public; Owner: dssp-admin
--

CREATE TABLE public.pdb_db_ref (
    id integer NOT NULL,
    pdb_id character varying NOT NULL,
    db_code character varying NOT NULL,
    db_name character varying NOT NULL,
    db_accession character varying NOT NULL
);


ALTER TABLE public.pdb_db_ref OWNER TO "dssp-admin";

--
-- Name: pdb_db_ref_id_seq; Type: SEQUENCE; Schema: public; Owner: dssp-admin
--

CREATE SEQUENCE public.pdb_db_ref_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.pdb_db_ref_id_seq OWNER TO "dssp-admin";

--
-- Name: pdb_db_ref_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: dssp-admin
--

ALTER SEQUENCE public.pdb_db_ref_id_seq OWNED BY public.pdb_db_ref.id;


--
-- Name: pdb_file; Type: TABLE; Schema: public; Owner: dssp-admin
--

CREATE TABLE public.pdb_file (
    id character varying NOT NULL,
    file_date timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL
);


ALTER TABLE public.pdb_file OWNER TO "dssp-admin";

--
-- Name: pdb_db_ref id; Type: DEFAULT; Schema: public; Owner: dssp-admin
--

ALTER TABLE ONLY public.pdb_db_ref ALTER COLUMN id SET DEFAULT nextval('public.pdb_db_ref_id_seq'::regclass);


--
-- Name: pdb_db_ref pdb_db_ref_pkey; Type: CONSTRAINT; Schema: public; Owner: dssp-admin
--

ALTER TABLE ONLY public.pdb_db_ref
    ADD CONSTRAINT pdb_db_ref_pkey PRIMARY KEY (id);


--
-- Name: pdb_file pdb_file_pkey; Type: CONSTRAINT; Schema: public; Owner: dssp-admin
--

ALTER TABLE ONLY public.pdb_file
    ADD CONSTRAINT pdb_file_pkey PRIMARY KEY (id);


--
-- Name: pdb_db_ref_ix_1; Type: INDEX; Schema: public; Owner: dssp-admin
--

CREATE INDEX pdb_db_ref_ix_1 ON public.pdb_db_ref USING btree (pdb_id);


--
-- Name: pdb_db_ref_ix_2; Type: INDEX; Schema: public; Owner: dssp-admin
--

CREATE INDEX pdb_db_ref_ix_2 ON public.pdb_db_ref USING btree (db_name);


--
-- Name: pdb_db_ref_ix_3; Type: INDEX; Schema: public; Owner: dssp-admin
--

CREATE INDEX pdb_db_ref_ix_3 ON public.pdb_db_ref USING btree (db_accession);


--
-- Name: pdb_db_ref pdb_db_ref_pdb_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: dssp-admin
--

ALTER TABLE ONLY public.pdb_db_ref
    ADD CONSTRAINT pdb_db_ref_pdb_id_fkey FOREIGN KEY (pdb_id) REFERENCES public.pdb_file(id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: SCHEMA public; Type: ACL; Schema: -; Owner: postgres
--

REVOKE USAGE ON SCHEMA public FROM PUBLIC;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

\unrestrict GzjWoLaKJbP8jb1ga0WXeZbYFwO3rdxhlh1hdJP35twZOPYAVG2fNdpeg5T1s83

