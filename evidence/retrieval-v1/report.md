# Frozen synthetic retrieval comparison v1

Author result, pending independent claim review. The unchanged 28-file/37-query CC0 corpus in benchmark/ was frozen before inference (inventory SHA2562fdb07dba3b3191a7073b7fa1443e58c542fa3c03e3275d429e54058c8996872). No label, query, threshold, engine source or ranking parameter was changed after observing results. Service717f97ce74e8c74e45821e6271e58ecb457b281acdb8a5815136d7dbf73e6eb1 and producerf34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf are previously accepted CPU candidates. Actual full pipeline/extractor identities are in platform.json and raw service status.

Default unfiltered service limit5 and native Baloo6.30 identical queries as typed were used. No semantic label was passed to search. Both output inventories have37 distinct query IDs. Baloo's default AND query semantics were preserved; there was no manual OR/term rewrite. All28 Baloo files report content/done; PNG extracted-term receipts have width/height metadata and empty plain-text terms, not OCR. Text/PDF content indexed successfully. Baloo literal controls8/8 and exact identifiers5/6 show actual content search; the missing identifier is in an OCR screenshot. This compares described-content queries on the declared corpus, not every KDE search syntax or ordinary filename search.

| Query subset | KDE Index first / top5 | Baloo first / top5 |
|---|---|---|
| Semantic text |6/8 /8/8|0/8 /0/8|
| Semantic PDF |4/4 /4/4|0/4 /0/4|
| Semantic images |7/8 /8/8|0/8 /0/8|
| All semantic |17/20 /20/20|0/20 /0/20|
| Unique identifiers |6/6 /6/6|5/6 /5/6|
| Literal controls |8/8 /8/8|8/8 /8/8|

On these20 semantic queries, file hit@5 is100% versus0%, a100 percentage-point difference. All4 semantic PDF results return the labeled source page. The predeclared >=80%, >=15pp and all-identifier targets pass this author scoring. This small authored corpus is intentionally semantic/paraphrase-oriented, with synthetic charts/screenshots rather than photographs; no user-population/general-photo quality estimate follows.

All three no-relevant queries return five irrelevant KDE Index suggestions (15 false positives); Baloo returns none. Scores range0.6844331329160638–0.734852601780104 across those suggestions and are ranking tiers, not probabilities. No relevance cutoff or reliable abstention has been established. The final app must retain its suggestions/check-source wording; these negative queries cannot be tuned into a cutoff on this evaluation.

## Target resources and clocks

AMD Ryzen53600,6 cores/12 threads, Linux7.2.7-1-cachyos-bore, CPU inference. Replays started after native author/reviewer processes closed and ran sequentially. Ordinary desktop background remained, with no CPU pinning or cache flush. Clock endpoints were acquired inside each bounded native script.185 warm repetitions per engine (five ordered passes of37) are latency observations, not185 additional retrieval queries. Service latency is send through matching decoded IPC response; Baloo latency includes CLI spawn through decoded results, so these scopes are disclosed rather than treated as identical inference costs.

| Measure | KDE Index | Baloo |
|---|---|---|
| Corpus indexing |21.844843353s,28 files/38 vectors,0 errors|6.36702165s,28 content-done files|
| Warm query p95 |42.05547ms|22.72413399987272ms|
| Sampled summed RSS peak |1127164KiB|69848KiB|
| Apparent index bytes |1412976|139264|

The service p95 is below the predeclared1s target on this small corpus. Three fixed queries after actual producer absence take2749.641418ms (text),2729.414559ms (PDF) and1956.676577ms (image). This includes verified model reload, not an OS cold-cache state. A Baloo model-unload phase does not apply. Index timings cover different workloads: this service adds OCR/embeddings; Baloo adds extracted terms/metadata. RSS uses50ms /proc service/daemon descendant sampling and sums resident pages per process; shared pages can be counted repeatedly and short peaks can be missed. Apparent index sizes include the own service DB/WAL files and Baloo data files at the declared endpoint; they are not disk allocated bytes or extrapolations to large folders.

Both scripts finished successfully. Service exit0, every known PID absent, socket absent. Baloo daemon was deliberately terminated after queries (exit-15), own sandbox and session bus closed; session/daemon logs are retained. No live desktop Baloo configuration/index, personal source folder, model byte or system configuration changed. Synthetic runtime indexes remain ignored; only raw results/logs/helpers are delivered. Independent review must challenge native-query construction/content completion, corpus/labels/hash custody, scoring/page association, latency/RSS scopes and closure. No installed GUI/KRunner or release/publication acceptance follows.
