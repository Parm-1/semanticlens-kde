# Real-film quality comparison: proposed S2

This is the follow-on plan from [issue #2](https://github.com/Parm-1/semanticlens-kde/issues/2),
prepared as part of S1 delivery. No real-film trial, provider call or bulk import
has been authorized or performed by this plan. Review S1 before starting S2.
Authored fixtures establish implementation behavior; they cannot establish that
LLM analysis improves discovery of real films.

## Question and matched comparison

Does imported LLM analysis improve text-grounded film discovery beyond source
search and curated synopsis metadata, while preserving literal retrieval and
defensible source references? Keep film and edition availability identical for
the matched comparison, and report missing-source/analysis behavior separately.

| Condition | Available retrieval evidence |
| --- | --- |
| A | Exact/lexical source baseline |
| B | Source lexical search and source embeddings |
| C | Source search and curated synopsis metadata |
| D | Source search and LLM analysis |
| E | Full hybrid |

These are proposed evaluation conditions, not five implemented product modes.
Version and qualify the ablation runner before using it for a claim. Neither
adding a synopsis nor simply adding more text demonstrates an LLM contribution.
Unverified priors remain excluded in all confirmatory conditions; an exploratory
prior view is a separately labeled experiment.

## Inputs and scoring

After approval, explicitly select up to 100 lawfully available films spanning
source type, language, era, genre, dialogue density and fame. Record editions,
source hashes, coverage and unavailable sources. Do not download material or
assume rights. Subtitle offsets are not verified playback alignment, and supplied
texts cannot establish acting, cinematography or other finished-film properties.

Author 100 queries before inspecting evaluation outputs: 30 implicit conceptual,
20 dialogue paraphrase, 20 unique literal/identifier, 10 multi-constraint,
10 misleading near-match and 10 no-answer. Include supported multilingual cases.
Freeze the 30-development/70-held-out split and use film-disjoint subsets where
feasible; disclose overlap. Keep test queries out of enrichment prompts and
cached analyses. Freeze corpus revisions, candidate limits, ranking weights,
filters, prompts and the scorer before the held-out run.

An independent reviewer reads the actual supporting source and labels film
relevance, passage relevance and explanation defensibility, with retrieval
condition concealed. The metadata-generating model cannot be the sole judge.
Qualify scoring with known source-supported positives and misleading or
unsupported matched controls. Scorer failure is a measurement failure, not a
retrieval null result. Retain negatives, missing answers and failed attempts in
the applicable denominators.

Report film-level nDCG@10 and Recall@10, source-passage Recall@5, unique-identifier
rank, unsupported-explanation rate, no-answer false positives, coverage, latency
and actual cost. Report paired differences and uncertainty with resampling that
respects query/film dependence. Specify the scorer and grouping before testing;
small or dependent samples may be inconclusive.

## Proposed decision gate

The design proposes default enrichment only if implicit-concept nDCG@10 improves
by at least 0.05 absolute over B with a positive paired uncertainty bound,
supported-passage Recall@5 falls by no more than 0.02, every deliberately unique
identifier remains correct, no stale or invalid source reference is presented as
valid, and no more than 5% of reviewed explanations are unsupported. These are
proposals, not measured results or an already accepted adoption decision. Freeze
or explicitly revise them before testing; do not adjust them after an unfavorable
hold-out result.

If the comparison fails or is inconclusive, retain the result and leave enrichment
experimental. Diagnose using development data. Any justified successor needs a
new declared protocol; do not repeatedly rerun the same holdout until it passes.
Absent an authorized real corpus, real-film quality measurement remains pending
without redefining the offline S1 implementation as incomplete.

Any new cloud authoring needs a separate grant specifying provider/model,
approved content and destination, film/token/currency ceilings, expiry, retries
and retention. S1's provider calls and monetary budget remain zero. Capacity
qualification and default rollout remain later S3 decisions.
