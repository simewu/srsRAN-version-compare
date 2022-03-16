A comparison tool to quantify all changes made in Bitcoin over the past few decades.

The script considers code files as those ending in [.cpp, .py, .c, .h, .sh, .sol, .go, .c, .js, .java].

## Results
| srsRAN Version                | Num all files | Size all files (B) | Num code files | Size code files (B) | \* | All line additions | All line removals | All files changed | Ratio all files changed | All changed bytes | Ratio all bytes changed | \* | Code line additions | Code line removals | Code files changed | Ratio code files changed | Code changed bytes | Ratio code bytes changed |
| ----------------------------- | ------------- | ------------------ | --------------------------------------------------- | ------------------- | -- | ------------------ | ----------------- | ----------------- | ----------------------- | ----------------- | ----------------------- | -- | ------------------- | ------------------ | ------------------ | ------------------------ | ------------------ | ------------------------ |
| srsRAN-release\_002\_000\_000 | 549           | 7144113            | 482                                                 | 6684189             | \* | N/A                | N/A               | N/A               | N/A                     | N/A               | N/A                     | \* | N/A                 | N/A                | N/A                | N/A                      | N/A                | N/A                      |
| srsRAN-release\_17\_09        | 571           | 7749258            | 503                                                 | 7096192             | \* | 21383              | 8593              | 318               | 55.6918%                | 3503893           | 45.2159%                | \* | 21108               | 7094               | 292                | 58.0517%                 | 3249146            | 45.7872%                 |
| srsRAN-release\_17\_12        | 618           | 8467474            | 543                                                 | 7801961             | \* | 28246              | 7120              | 340               | 55.0162%                | 6386938           | 75.4291%                | \* | 27644               | 6900               | 313                | 57.6427%                 | 6306087            | 80.8269%                 |
| srsRAN-release\_18\_03        | 618           | 8570891            | 543                                                 | 7904845             | \* | 5141               | 2334              | 159               | 25.7282%                | 3418182           | 39.8813%                | \* | 5082                | 2317               | 151                | 27.8085%                 | 3369714            | 42.6285%                 |
| srsRAN-release\_18\_03\_1     | 618           | 8578030            | 543                                                 | 7911938             | \* | 870                | 845               | 278               | 44.9838%                | 3230705           | 37.6626%                | \* | 859                 | 838                | 272                | 50.0921%                 | 3220329            | 40.7022%                 |
| srsRAN-release\_18\_06        | 649           | 8999162            | 564                                                 | 8293572             | \* | 19793              | 6913              | 303               | 46.6872%                | 6574956           | 73.0619%                | \* | 18546               | 6642               | 266                | 47.1631%                 | 6412981            | 77.3247%                 |
| srsRAN-release\_18\_06\_1     | 651           | 8980624            | 564                                                 | 8297644             | \* | 359                | 600               | 41                | 6.2980%                 | 700133            | 7.7960%                 | \* | 196                 | 63                 | 29                 | 5.1418%                  | 671489             | 8.0925%                  |
| srsRAN-release\_18\_09        | 652           | 9076873            | 565                                                 | 8391405             | \* | 9505               | 6079              | 207               | 31.7485%                | 3130406           | 34.4877%                | \* | 9347                | 3959               | 185                | 32.7434%                 | 3063429            | 36.5067%                 |
| srsRAN-release\_18\_12        | 684           | 15349464           | 582                                                 | 14655314            | \* | 233625             | 29839             | 221               | 32.3099%                | 11664060          | 75.9900%                | \* | 233083              | 7389               | 172                | 29.5533%                 | 11571373           | 78.9568%                 |
| srsRAN-release\_19\_03        | 682           | 15058290           | 584                                                 | 14372206            | \* | 62729              | 87190             | 657               | 96.3343%                | 13697400          | 90.9625%                | \* | 62173               | 75812              | 536                | 91.7808%                 | 13522334           | 94.0867%                 |
| srsRAN-release\_19\_06        | 724           | 15120882           | 621                                                 | 14426916            | \* | 52107              | 50872             | 346               | 47.7901%                | 12389971          | 81.9395%                | \* | 51718               | 48114              | 304                | 48.9533%                 | 12294988           | 85.2226%                 |
| srsRAN-release\_19\_09        | 795           | 15590434           | 686                                                 | 14883907            | \* | 28570              | 12763             | 298               | 37.4843%                | 10254355          | 65.7734%                | \* | 28095               | 11846              | 271                | 39.5044%                 | 10176993           | 68.3758%                 |
| srsRAN-release\_19\_12        | 892           | 41415591           | 764                                                 | 20783393            | \* | 228489             | 49358             | 721               | 80.8296%                | 40483357          | 97.7491%                | \* | 227807              | 47348              | 671                | 87.8272%                 | 20458224           | 98.4354%                 |
| srsRAN-release\_20\_04        | 993           | 31906640           | 850                                                 | 23243592            | \* | 192596             | 109369            | 930               | 93.6556%                | 30743599          | 96.3549%                | \* | 191610              | 58134              | 796                | 93.6471%                 | 22980455           | 98.8679%                 |
| srsRAN-release\_20\_04\_1     | 996           | 31891398           | 853                                                 | 23228498            | \* | 3155               | 4199              | 91                | 9.1365%                 | 4007870           | 12.5672%                | \* | 3138                | 4183               | 85                 | 9.9648%                  | 3951538            | 17.0116%                 |
| srsRAN-release\_20\_04\_2     | 996           | 31890628           | 853                                                 | 23227566            | \* | 14                 | 23                | 7                 | 0.7028%                 | 128243            | 0.4021%                 | \* | 9                   | 22                 | 5                  | 0.5862%                  | 120349             | 0.5181%                  |
| srsRAN-release\_20\_10        | 1146          | 32981298           | 997                                                 | 24319677            | \* | 57989              | 24131             | 606               | 52.8796%                | 10675974          | 32.3698%                | \* | 57139               | 15145              | 541                | 54.2628%                 | 10444106           | 42.9451%                 |
| srsRAN-release\_20\_10\_1     | 1146          | 32981299           | 997                                                 | 24319618            | \* | 8                  | 7                 | 6                 | 0.5236%                 | 94872             | 0.2877%                 | \* | 3                   | 5                  | 3                  | 0.3009%                  | 66049              | 0.2716%                  |
| srsRAN-release\_21\_04\_pre   | 1495          | 44432804           | 1328                                                | 29106137            | \* | 474593             | 350843            | 1555              | 104.0134%               | 36016274          | 81.0578%                | \* | 469732              | 141366             | 1307               | 98.4187%                 | 29026428           | 99.7261%                 |
| srsRAN-release\_21\_04        | 1496          | 44441464           | 1329                                                | 29114451            | \* | 325                | 116               | 34                | 2.2727%                 | 674015            | 1.5166%                 | \* | 309                 | 115                | 29                 | 2.1821%                  | 621632             | 2.1351%                  |
| srsRAN-release\_21\_10        | 1684          | 55886340           | 1489                                                | 31371407            | \* | 79203              | 17434             | 1006              | 59.7387%                | 23161592          | 41.4441%                | \* | 77921               | 12237              | 888                | 59.6373%                 | 13752636           | 43.8381%                 |

## File Extension Histogram
| srsRAN Version                | File extenension histogram                                                                                                                                                                                                                                             |
| ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| srsRAN-release\_002\_000\_000 | .h (200), .c (166), .cc (89), .txt (59), .cmake (18), .example (5),  (3), .in (3), .dat (3), .md (1), .mexa64 (1), .cpp (1)                                                                                                                                            |
| srsRAN-release\_17\_09        | .h (211), .c (176), .cc (89), .txt (59), .cmake (18), .example (5),  (3), .in (3), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1)                                                                                                                                  |
| srsRAN-release\_17\_12        | .h (227), .c (179), .cc (106), .txt (66), .cmake (17), .example (7), .in (5),  (3), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                                        |
| srsRAN-release\_18\_03        | .h (227), .c (179), .cc (106), .txt (66), .cmake (17), .example (7), .in (5),  (3), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                                        |
| srsRAN-release\_18\_03\_1     | .h (227), .c (179), .cc (106), .txt (66), .cmake (17), .example (7), .in (5),  (3), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                                        |
| srsRAN-release\_18\_06        | .h (233), .c (181), .cc (115), .txt (69), .cmake (18),  (9), .example (9), .in (6), .dat (3), .md (1), .install (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                          |
| srsRAN-release\_18\_06\_1     | .h (233), .c (181), .cc (115), .txt (69), .cmake (18),  (12), .example (9), .in (6), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                                       |
| srsRAN-release\_18\_09        | .h (237), .c (178), .cc (115), .txt (69), .cmake (18),  (12), .example (9), .in (6), .dat (3), .md (1), .mexa64 (1), .bin (1), .cpp (1), .sh (1)                                                                                                                       |
| srsRAN-release\_18\_12        | .h (241), .c (178), .cc (125), .txt (72), .cmake (18),  (10), .example (9), .in (6), .install (5), .manpages (3), .service (3), .dat (3), .md (2), .sh (2), .yml (1), .config (1), .postinst (1), .templates (1), .mexa64 (1), .bin (1), .cpp (1)                      |
| srsRAN-release\_19\_03        | .h (255), .c (158), .cc (131), .txt (69), .cmake (19),  (10), .example (9), .in (6), .install (5), .manpages (3), .service (3), .dat (3), .md (2), .sh (2), .cpp (2), .yml (1), .config (1), .postinst (1), .templates (1), .bin (1)                                   |
| srsRAN-release\_19\_06        | .h (277), .c (162), .cc (142), .txt (74), .cmake (19),  (10), .example (9), .in (6), .install (5), .manpages (3), .service (3), .dat (3), .md (2), .sh (2), .cpp (2), .yml (1), .config (1), .postinst (1), .templates (1), .bin (1)                                   |
| srsRAN-release\_19\_09        | .h (303), .c (178), .cc (162), .txt (79), .cmake (20),  (10), .example (9), .in (6), .install (5), .sh (4), .md (3), .manpages (3), .service (3), .dat (3), .cpp (2), .yml (1), .config (1), .postinst (1), .templates (1), .bin (1)                                   |
| srsRAN-release\_19\_12        | .h (332), .c (204), .cc (184), .txt (80), .cmake (20), .dat (13),  (11), .example (9), .bin (7), .in (6), .install (5), .sh (4), .yml (3), .md (3), .manpages (3), .service (3), .cpp (2), .config (1), .postinst (1), .templates (1)                                  |
| srsRAN-release\_20\_04        | .h (373), .c (232), .cc (198), .txt (85), .dat (22), .cmake (21),  (11), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .cpp (2), .config (1), .postinst (1), .templates (1)                                  |
| srsRAN-release\_20\_04\_1     | .h (375), .c (232), .cc (199), .txt (85), .dat (22), .cmake (21),  (11), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .cpp (2), .config (1), .postinst (1), .templates (1)                                  |
| srsRAN-release\_20\_04\_2     | .h (375), .c (232), .cc (199), .txt (85), .dat (22), .cmake (21),  (11), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .cpp (2), .config (1), .postinst (1), .templates (1)                                  |
| srsRAN-release\_20\_10        | .h (463), .cc (239), .c (236), .txt (90), .dat (22), .cmake (21), .cpp (14),  (11), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .config (1), .postinst (1), .templates (1), .rst (1)                       |
| srsRAN-release\_20\_10\_1     | .h (463), .cc (239), .c (236), .txt (90), .dat (22), .cmake (21), .cpp (14),  (11), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .config (1), .postinst (1), .templates (1), .rst (1)                       |
| srsRAN-release\_21\_04\_pre   | .h (626), .c (323), .cc (314), .txt (105), .dat (24), .cmake (22), .cpp (18),  (12), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .config (1), .postinst (1), .templates (1), .hpp (1), .rst (1)            |
| srsRAN-release\_21\_04        | .h (626), .c (323), .cc (314), .txt (105), .dat (24), .cmake (23), .cpp (18),  (12), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .config (1), .postinst (1), .templates (1), .hpp (1), .rst (1)            |
| srsRAN-release\_21\_10        | .h (710), .cc (366), .c (344), .txt (120), .dat (35), .cmake (24), .cpp (20),  (12), .example (9), .bin (7), .sh (6), .in (6), .install (5), .md (4), .yml (3), .manpages (3), .service (3), .data (2), .config (1), .postinst (1), .templates (1), .hpp (1), .rst (1) |



The color coded table can be found at [logDirectoryOutput.xlsx](logDirectoryOutput.xlsx).
