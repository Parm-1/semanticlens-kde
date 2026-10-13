# Top-level install repair

Commit 207dac64c5d48454f259b1d40a6fcdc9c787e2e6 changes only top-level installation rules. Reconfiguration and incremental build succeeded; installation now has all 24 expected runtime/notice files and all seven required executable/module paths, without runtime SDK headers or libllama.a. Original failed 22-file stage is unchanged. No model/UI/live installation ran.

Cleanly built GUI/service/producer/tokenizer/worker hashes differ from the validated native stage. Their build success does not transfer binary-level numerical, concurrency, provenance or latency qualification. Media, CLI, plugin and installed metadata/notices match the validated stage. The known dff worker qualification remains hash-specific; this rebuilt worker reports unqualified provenance under the same source gate. Rebuilt-producer reference checks are separately recorded.
