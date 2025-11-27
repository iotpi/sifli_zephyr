board_runner_args(probe-rs "--chip=SF32LB58" "--speed=4000" "--protocol=swd")

include(${ZEPHYR_BASE}/boards/common/probe-rs.board.cmake)
