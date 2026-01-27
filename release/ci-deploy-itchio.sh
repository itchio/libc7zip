#!/bin/bash -xe

if [ "${GITHUB_REF_TYPE}" == "tag" ]; then
  # pushing a stable version
  export CHANNEL_SUFFIX=""
  export USER_VERSION=`echo ${GITHUB_REF_NAME} | tr -d "v"` # v9.0.0 => 9.0.0
elif [ "master" == "${GITHUB_REF_NAME}" ]; then
  # pushing head
  export CHANNEL_SUFFIX="-head"
  export USER_VERSION="${GITHUB_SHA}"
else
  # pushing a branch that isn't master
  echo "Not pushing non-master branch ${GITHUB_REF_NAME}"
  exit 0
fi

# upload to itch.io
export TOOLS_DIR=$PWD/tools/
mkdir -p ${TOOLS_DIR}
pushd ${TOOLS_DIR}
curl -sLo butler.zip "https://broth.itch.zone/butler/linux-amd64-head/LATEST/.zip"
unzip butler.zip
popd

${TOOLS_DIR}/butler -V

pushd broth
for i in *; do
    # Check if this platform should be deployed
    case "$i" in
        broth-linux-amd64)  [ "$DEPLOY_LINUX_AMD64" != "true" ] && continue ;;
        broth-linux-arm64)  [ "$DEPLOY_LINUX_ARM64" != "true" ] && continue ;;
        broth-windows-386)  [ "$DEPLOY_WINDOWS_386" != "true" ] && continue ;;
        broth-windows-amd64) [ "$DEPLOY_WINDOWS_AMD64" != "true" ] && continue ;;
        broth-darwin-amd64) [ "$DEPLOY_DARWIN_AMD64" != "true" ] && continue ;;
        broth-darwin-arm64) [ "$DEPLOY_DARWIN_ARM64" != "true" ] && continue ;;
    esac

    CHANNEL_NAME="${i}${CHANNEL_SUFFIX}"
    ${TOOLS_DIR}/butler push --userversion "${USER_VERSION}" ./$i "itchio/libc7zip:${CHANNEL_NAME}"
done
popd
