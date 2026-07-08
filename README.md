# Yext2fs - Ext2 FileSystem の再実装

[筑波大学 情報システム実験: 「カーネルハック」](https://www.coins.tsukuba.ac.jp/~yas/coins/slab-kernel-2026/) において実装

Ext2 の基本的な機能を再実装する

## 実装状況

### FUSE

現在はまずFUSEで実装している

- [ ] ディレクトリ
  - [ ] 指定したディレクトリパス以下に何のエントリがあるか読める
    - [x] `/` 直下
    - [ ] 任意のディレクトリパス
  - [ ] 指定したディレクトリパスを作成できる(`-p`なし`mkdir`できる)
- [ ] ファイル
  - [ ] 読める
  - [ ] 書ける
- [ ] エントリの属性
- [ ] シンボリックリンク等

```sh
$ dd if=/dev/zero of=ext2.img bs=1024 count=1024
$ mkfs.ext2 ext2.img
```

などとして Ext2 のイメージを作成。

```sh
$ ./yext2 ./mnt ./ext2.img
```

のように起動すると、`./mnt`ディレクトリ以下に Ext2 のイメージがFUSEによりマウントされ、
通常のファイルシステムのような操作が Yext2 を介して行われる。

### kernel land

後で移植予定
