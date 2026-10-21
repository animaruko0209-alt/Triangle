# DirectX 11 テクスチャ・点光源・鏡面反射デモ

Visual Studio 2022 / Windows SDK / DirectXTK (NuGet) を使用します。
`Project1.sln` の Debug / x64 をビルドしてください。PNGとModelsフォルダーはビルド先にもコピーされます。
画像とモデルは作業フォルダー、次に実行ファイルのフォルダーから検索します。

- 円柱は底面・上面・側面の頂点を分け、底面と上面に平面UV、側面に周回UVを設定しています。
- `check.png` / `check2.png`、`mask5b.png`、`mask5b_normal.png` を使用します。
- 点光源の拡散反射・鏡面反射を別々に計算し、距離 / 照射範囲によって減衰します。
- 円柱は金色の反射、立方体・直方体は黄色い材質と白い反射、奥の円柱は白い反射です。
- 点光源の `pointLightRange`、`pointLightIntensity`、`pointLightColor` と、材質の
  `specularColor`、`specularPower`、`specularIntensity` は `FileName.cpp` で設定できます。

| 操作 | 動作 |
| --- | --- |
| 左ドラッグ | 原点の周囲で視点を回転 |
| ホイール、I / O | カメラの距離を変更 |
| A / D / W / S | カメラを左右 / 上下に移動 |
| H | カメラを初期位置に戻す |
| 右ドラッグ | 点光源をX / Z方向に移動 |
| J / L | 点光源を左右に移動 |
| K | 点光源を初期位置に戻す |
| 9 / 0 | OBJから読み込んだ立方体 / 球 |
| P | OBJから読み込んだ多面体（platonic.obj） |
| C | 立方体 (1 × 1 × 1) |
| V / B / N | 直方体 (1 × 2 × 3 / 2 × 3 × 1 / 3 × 1 × 2) |
| 3〜8 | 手前の円柱の角数を変更 |
| 矢印キー | 手前の円柱を移動 |
| 1 / 2 | 手前の円柱を縮小 / 拡大 |
| F / G | 手前の円柱を奥 / 手前に移動 |
| X / Y / Z | 円柱の回転軸を変更 |
| Q | 終了 |

立方体・直方体の位置、回転軸、回転速度は共通です。光源位置は小さな発光マーカーで表示します。
Bは資料に合わせて直方体切り替えに使用し、従来の後退操作はGに移しています。

参考資料: `MkCylinder_Vertex3_new.pdf`、`DirectX11_Diffuse_Specular4.pdf`。

## OBJモデル

`Models/cube.obj`、`Models/sphere.obj` と、ZIP内の `Models/platonic.obj` は添付データをそのまま収録しています。
9 / 0 / Pキーで左側の回転オブジェクトを立方体 / 球 / 多面体のOBJモデルに切り替え、Cで生成した立方体に戻せます。
位置・回転速度・PNG材質は共通です。OBJの元データは保持し、表示時に中心と大きさを揃えます。
多角形の三角形化、UVのV反転、法線の読み込み、接線とUVの向きの計算を行います。
法線がない場合は面法線を生成し、UVがない・重複する場合にも安定した接線を使います。
MTL材質の自動取り込みは行わず、既存のPNGと鏡面反射を使用します。

依存ライブラリは公式の [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) から取得済みです。
`third_party/tinyobjloader` にヘッダー、MITライセンス、取得コミットを同梱しています。
再ダウンロードせずビルドできます。モデルの *.obj はGitで追跡できるよう除外設定を調整しています。
参考資料: `objLoader.pdf`、今回添付の `DirectX11_OBJ_Reader.pdf`（`DirectX11_OBJ_Reader_old.pdf` は旧版）。
資料のOBJ読み込み・接線計算を既存ローダーと照合し、現在の描画処理に統合しています。
