-- This are German tax models.
-- LemonPOS version 1.0 preview
-- TuxTrainer
-- Licensed as GPL v2 or later.

USE lemonposdb;

INSERT INTO lemonposdb.taxmodels (modelid,tname,elementsid) VALUES(1,"19% MwSt", "1");
INSERT INTO lemonposdb.taxelements (elementid, ename, amount) VALUES (1,"19% MwSt", 19);

INSERT INTO lemonposdb.taxmodels (modelid,tname,elementsid) VALUES(2,"7% MwSt","2");
INSERT INTO lemonposdb.taxelements (elementid, ename, amount) VALUES (2,"7% MwSt", 7);
