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
curl -sLo butler.zip "https://broth.itch.ovh/butler/linux-amd64-head/LATEST/.zip"
unzip butler.zip
popd

${TOOLS_DIR}/butler -V

pushd broth
for i in *; do
    CHANNEL_NAME="${i}${CHANNEL_SUFFIX}"
    ${TOOLS_DIR}/butler push --userversion "${USER_VERSION}" ./$i "itchio/libc7zip:${CHANNEL_NAME}"
done
popd
