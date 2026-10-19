name: Build PS4 PKG

# يشتغل يدويًا من تبويب Actions (Run workflow) وكل ما ترفع تعديل على main
on:
  workflow_dispatch:
  push:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-22.04

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang lld llvm curl jq tar unzip

      - name: Download OpenOrbis toolchain (latest release)
        env:
          GH_TOKEN: ${{ github.token }}
        run: |
          set -e
          mkdir -p "$HOME/ps4dev"
          cd "$HOME/ps4dev"

          API="https://api.github.com/repos/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/latest"
          curl -sL -H "Authorization: Bearer $GH_TOKEN" "$API" -o release.json

          TAG=$(jq -r '.tag_name' release.json)
          echo "Latest toolchain tag: $TAG"
          echo "Assets:"; jq -r '.assets[].name' release.json

          # نختار أول ملف ضغط ما هو لماك/ويندوز
          URL=$(jq -r '.assets[].browser_download_url' release.json \
                | grep -iE '(\.tar\.gz|\.zip)$' \
                | grep -ivE 'mac|osx|win|exe|installer' | head -n1 || true)

          # احتياط: الاسم القديم المعروف للأصل (v0.5.x.tar.gz)
          if [ -z "$URL" ]; then
            VER="${TAG#v}"
            URL="https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/${TAG}/v${VER}.tar.gz"
          fi
          echo "Downloading: $URL"
          curl -sL "$URL" -o toolchain_archive

          case "$URL" in
            *.zip)    unzip -q toolchain_archive ;;
            *)        tar xzf toolchain_archive ;;
          esac

          # نلقى جذر الأدوات من مكان samples/hello_world
          HW=$(find . -type d -path '*/samples/hello_world' | head -n1)
          if [ -z "$HW" ]; then
            echo "ERROR: samples/hello_world not found in toolchain archive"
            find . -maxdepth 3 -type d | head -50
            exit 1
          fi
          ROOT=$(realpath "${HW%/samples/hello_world}")
          echo "Toolchain root: $ROOT"
          echo "OO_PS4_TOOLCHAIN=$ROOT" >> "$GITHUB_ENV"

          chmod -R +x "$ROOT"/bin/linux/* 2>/dev/null || true

          # سكربت الإعداد للينكس (لو موجود)
          SETUP=$(find "$ROOT" -name 'setup-toolchain.sh' | head -n1 || true)
          if [ -n "$SETUP" ]; then
            echo "Running $SETUP"
            OO_PS4_TOOLCHAIN="$ROOT" bash "$SETUP" || echo "setup script failed (continuing)"
          fi

      - name: Prepare project from the toolchain's hello_world sample
        run: |
          set -e
          cd "$GITHUB_WORKSPACE"
          rm -rf work
          cp -r "$OO_PS4_TOOLCHAIN/samples/hello_world" work
          cd work

          # نشيل كود العينة ونحط كودنا
          rm -f *.c *.cpp
          cp ../src/main.c .

          echo "----- Makefile (first 40 lines) -----"
          head -40 Makefile
          echo "-------------------------------------"

          # معلومات التطبيق (CONTENT_ID لازم 36 حرف بالضبط)
          sed -i 's/^TITLE[[:space:]]*:=.*/TITLE      := Xbox Controller Test/' Makefile
          sed -i 's/^TITLE_ID[[:space:]]*:=.*/TITLE_ID   := XBRC00001/' Makefile
          sed -i 's/^CONTENT_ID[[:space:]]*:=.*/CONTENT_ID := IV0000-XBRC00001_00-XBOXCTRLTEST0000/' Makefile

          # المكتبات: اليد + المستخدم
          sed -i 's/^LIBS[[:space:]]*:=.*/LIBS       := -lc -lkernel -lc++ -lScePad -lSceUserService/' Makefile

      - name: Build
        run: |
          cd "$GITHUB_WORKSPACE/work"
          make

      - name: Check PKG exists
        run: |
          cd "$GITHUB_WORKSPACE/work"
          ls -la
          ls *.pkg

      - name: Upload PKG
        uses: actions/upload-artifact@v4
        with:
          name: xbox-controller-test-pkg
          path: work/*.pkg
