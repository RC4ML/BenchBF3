LDFLAGS_COMMON := -s -w

GOARCH=amd64 go build -ldflags "-s -w"  -o build/ec_bench_sw_amd64 ./main.go
GOARCH=arm64 go build -ldflags "-s -w"  -o build/ec_bench_sw_arm64 ./main.go