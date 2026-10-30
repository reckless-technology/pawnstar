# Third-party notices

Pawnstar is distributed under the GNU General Public License, version 3 (see [LICENSE](LICENSE)). Two things
in it carry a third party's notice, and both notices follow.

- **The shipped network.** `nnue/pawnstar-v20.bin` was trained entirely on a database published under the
  Open Database License, which obliges a notice naming the source wherever a work trained on it is
  distributed. That is the first section below, and it applies to every copy of Pawnstar — source tree or
  release binary.
- **Two files of trainer source.** `tools/bullet/pawnstar.rs` and `tools/bullet/pawnstar_eval.rs` are adapted
  from MIT-licensed code, whose copyright and permission notice must accompany them. That is the second
  section.

## Leela Chess Zero self-play training data

**The shipped network `nnue/pawnstar-v20.bin` was trained entirely on the Leela Chess Zero self-play training
data**, and so is every network Pawnstar ships from release 2.0.0 onward. That data is published as a database
under the Open Database License, so this notice accompanies the network wherever it goes.

- **Source:** Leela Chess Zero self-play training data, <https://storage.lczero.org/files/training_data/>
- **Database licence:** Open Database License 1.0 (`ODbL-1.0`),
  <http://opendatacommons.org/licenses/odbl/1.0/>
- **Contents licence:** Database Contents License 1.0 (`DbCL-1.0`),
  <http://opendatacommons.org/licenses/dbcl/1.0/>
- **The runs the v20 training pool was built from:** `test91`, 62.83% of its 6,143,698,748 positions, and
  `test60`, the other 37.17%. Both were extracted with `tools/lc0_extract`, which records the licence, the
  source URL and the run in a provenance sidecar beside every training shard it writes.

The attribution, in the form `tools/lc0_extract` writes into each of those sidecars, one line per run:

> Position data from the Leela Chess Zero self-play training data
> (https://storage.lczero.org/files/training_data/), run test91, used under the Open Database License 1.0
> (http://opendatacommons.org/licenses/odbl/1.0/).
>
> Position data from the Leela Chess Zero self-play training data
> (https://storage.lczero.org/files/training_data/), run test60, used under the Open Database License 1.0
> (http://opendatacommons.org/licenses/odbl/1.0/).

**What this notice means, and what it does not.** A trained network is a *Produced Work* under ODbL §1.0. The
obligation that attaches to distributing a Produced Work is **this notice** — naming the source database and its
licence — and that is the whole of it.

- **ODbL does not require the network itself to be licensed under ODbL.** `nnue/pawnstar-v20.bin` is not a
  database and is not offered under ODbL; it is a Produced Work derived from an ODbL database, and the licence
  places no licensing condition on a Produced Work. ODbL's share-alike applies to the database, not to what is
  learnt from it.
- **Nor does the network become GPL merely because this project's GPLv3 code and tooling trained it.** The GPL
  covers Pawnstar's program and its source; the network file is data shipped alongside it, and neither licence
  makes the other reach it.
- **No part of the database is redistributed here.** The archives are downloaded from the address above;
  `tools/lc0_extract` reads them as a stream and writes training shards, and neither the archives nor the
  shards are in this repository or in any release.

The licence text below is the `LICENSE` member that every training-data tar carries, quoted unaltered (it is
also quoted in `tools/lc0_extract.h`, where the reasoning above is recorded in full):

```text
This collection of training data for Leela Chess Zero is made available under the Open Database
License: http://opendatacommons.org/licenses/odbl/1.0/. Any rights in individual contents of the
database are licensed under the Database Contents License:
http://opendatacommons.org/licenses/dbcl/1.0/
```

## bullet

Source: <https://github.com/jw1912/bullet>, the commit pinned as `BULLET_COMMIT` in `tools/setup_bullet.sh`
(`da426a49b2a4b79a55a87031cd51c89bdf1ff030`). The text below is that commit's `LICENSE`, unaltered.

Adapted here:

- `tools/bullet/pawnstar.rs`, the NNUE trainer, is adapted from bullet's `examples/simple.rs`. The imports, the
  `QA`, `QB` and `SCALE` constants, the `SavedFormat` list, the loss function, the two-layer SCReLU network
  definition and the layout of the training schedule, settings and data loader follow that example. What is
  Pawnstar's own: the network shape (hidden width, mirrored king buckets, material output heads), the
  transposed output layer and its 32-bit bias, and the environment-variable configuration.
- `tools/bullet/pawnstar_eval.rs`, the reference evaluator, follows the same example's SCReLU inference, the
  field layout of its `Network` struct and its dequantisation order, and takes the per-piece feature index and
  the king-bucket offset from bullet's `Chess768` and `ChessBuckets` input types
  (`crates/bullet_lib/src/game/inputs/chess768.rs` and `chess_buckets.rs`). The net loader, the FEN driver and
  the rest are Pawnstar's own.

bullet itself is not part of this repository: `tools/setup_bullet.sh` fetches it, and installs these two files
into it as examples.

```text
MIT License

Copyright (c) 2023 Jamie Whiting

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
