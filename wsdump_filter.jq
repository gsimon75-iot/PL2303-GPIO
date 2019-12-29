#!/usr/local/bin/jq -f

map(._source.layers | {
	"idx": .frame."frame.number",
	"time": .frame."frame.time_relative",
	"ep": .usb."usb.endpoint_address",
	"data_out": .usb."Frame 0"."usb.frame.data",
	"data_in": .usb."Frame 1"."usb.frame.data"
})
