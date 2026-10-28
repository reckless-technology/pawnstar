# Third-party notices

Pawnstar is distributed under the GNU General Public License, version 3 (see [LICENSE](LICENSE)). Two files
in it are adapted from MIT-licensed code, whose copyright and permission notice must accompany them. That
notice follows.

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
