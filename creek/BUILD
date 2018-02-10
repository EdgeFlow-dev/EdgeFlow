#git subtree

REMOTE_ADDR=git@bitbucket.org:bakhi/xplane_nw.git
REMOTE_BR=xzl/mempool
PREFIX=xplane

REMOTE_ADDR=git@bitbucket.org:bakhi/zmq_stream.git
REMOTE_BR=master
PREFIX=zmq_stream

git remote add ${PREFIX} ${REMOTE_ADDR}
git subtree add --prefix=${PREFIX} ${PREFIX} ${REMOTE_BR}

# cd $mypaper
git subtree push --prefix=${PREFIX} ${REMOTE_ADDR} ${REMOTE_BR}

# to pull from remote
git subtree pull --prefix=${PREFIX} ${REMOTE_ADDR} ${REMOTE_BR}

