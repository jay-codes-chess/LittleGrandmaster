#!/bin/bash
./LittleGrandmaster <<'END_UCI'
position startpos
go depth 4
position startpos moves d8d1
go depth 4
position startpos moves d8d1 g8f6
go depth 4
quit
END_UCI