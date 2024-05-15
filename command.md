## dt 设置/返回 日期时间

    dt

    dt yyyy/mm/dd hh:nn:ss

## loc / locale 设置/返回语言

    {"voice_id": 0,
        "locales": [
        {"id":0, "lcid":"en-AU", "voice":"Chloe"},
        {"id":4, "lcid":"en-US", "voice":"Ava"},
        {"id":5, "lcid":"en-US", "voice":"Ethan"}
    ]}

命令:
    loc
    loc <id>
    loc <lcid>

示例:
    loc         返回语言设置和列表
    loc en      设置英文
    loc en-AU   设置英文-AU
    loc 4       设置英文-US

# hfmt 设置时间格式

    hfmt 0      语言默认
    hfmt 12     12小时制
    hfmt 24     24t小时制

## vol/volume 设置/返回音量

    vol 100     设置 100% 音量
    vol 90      设置 90% 音量


## alm/alarm 设置返回闹钟

    {"alarms": [
            {"id":1, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":2, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":3, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":4, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":5, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":6, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":7, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":8, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0}
    ]}

命令:
    alm
    alm <id> <enable/disable>
    alm <id> <enable/disable> <mtime> <ringtone_id> [mdate=yyyymmdd] [wdays=%x]

示例:
    alm                             返回闹钟设置
    alm 1 disable                   禁止闹钟
    alm 1 enable 1700 0 wdays=0x7F  设置每天下午5点
    alm 1 enable 1700 0 wdays=0x3E  设置工作日(周一至周五) 下午5点


# lamp 夜灯控制

    {"colors": [
        {"id": 0, "R": 50, "G": 50, "B": 50}
    ]}

命令:
    lamp                            返回色表
    lamp on / off                   开关夜灯
    lamp [id] [20~100]              开关夜灯
