/*
  HexaOS - page_root.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  HTML for GET /. Full admin dashboard layout using Alpine.js (loaded
  from /assets/alpine.min.js — served from PROGMEM, no CDN required).
  Embedded in firmware PROGMEM, not served from LittleFS.
*/

#pragma once

#include "headers/hx_build.h"

static const char PAGE_ROOT[] PROGMEM =
  "<!DOCTYPE html>"
  "<html lang=\"en\">"
  "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>" HX_SYSTEM_NAME "</title>"
    "<script src=\"/assets/alpine.min.js\" defer></script>"
    "<style>"
      "*{box-sizing:border-box;margin:0;padding:0}"
      "body{font-family:monospace;background:#0d0d0d;color:#e0e0e0;"
           "display:flex;flex-direction:column;height:100vh;overflow:hidden}"

      /* topbar */
      "#topbar{display:flex;align-items:center;justify-content:space-between;"
              "padding:0 1.5rem;height:3rem;background:#161616;"
              "border-bottom:1px solid #2a2a2a;flex-shrink:0}"
      "#topbar .logo{font-size:1.1rem;font-weight:bold;letter-spacing:.05em;color:#fff}"
      "#topbar .meta{display:flex;gap:1.5rem;font-size:.8rem;color:#888}"
      "#topbar .dot{width:.55rem;height:.55rem;border-radius:50%;display:inline-block;"
                   "margin-right:.4rem;background:#444}"
      "#topbar .dot.online{background:#3fb950}"
      "#topbar .dot.offline{background:#f85149}"

      /* layout */
      "#layout{display:flex;flex:1;overflow:hidden}"

      /* sidebar */
      "#sidebar{width:11rem;background:#111;border-right:1px solid #2a2a2a;"
               "display:flex;flex-direction:column;padding:1rem 0;flex-shrink:0}"
      "#sidebar a{padding:.55rem 1.25rem;color:#888;text-decoration:none;"
                 "font-size:.85rem;cursor:pointer;transition:color .15s,background .15s}"
      "#sidebar a:hover,#sidebar a.active{color:#e0e0e0;background:#1e1e1e}"
      "#sidebar a.active{border-left:2px solid #58a6ff;padding-left:calc(1.25rem - 2px)}"

      /* console */
      "#console-wrap{display:flex;flex-direction:column;height:100%}"
      "#console-out{flex:1;overflow-y:auto;background:#0a0a0a;border:1px solid #2a2a2a;"
                   "border-radius:.4rem .4rem 0 0;padding:.75rem;font-size:.8rem;"
                   "line-height:1.5;color:#c9d1d9;white-space:pre-wrap;word-break:break-all}"
      "#console-out .line-err{color:#f85149}"
      "#console-out .line-ok{color:#3fb950}"
      "#console-in{display:flex;border:1px solid #2a2a2a;border-top:none;"
                  "border-radius:0 0 .4rem .4rem;overflow:hidden}"
      "#console-in span{padding:.5rem .75rem;color:#8b949e;background:#111;font-size:.8rem;"
                       "border-right:1px solid #2a2a2a;flex-shrink:0}"
      "#console-in input{flex:1;background:#111;border:none;outline:none;color:#e0e0e0;"
                        "font-family:monospace;font-size:.8rem;padding:.5rem .75rem}"

      /* main */
      "#main{flex:1;overflow-y:auto;padding:1.5rem}"
      "h2{font-size:1rem;color:#8b949e;text-transform:uppercase;"
         "letter-spacing:.08em;margin-bottom:1rem}"

      /* cards */
      ".cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(11rem,1fr));"
             "gap:.75rem;margin-bottom:1.5rem}"
      ".card{background:#161616;border:1px solid #2a2a2a;border-radius:.5rem;"
            "padding:1rem}"
      ".card .label{font-size:.75rem;color:#8b949e;margin-bottom:.35rem}"
      ".card .value{font-size:1.4rem;font-weight:bold;color:#e6edf3}"
      ".card .sub{font-size:.75rem;color:#8b949e;margin-top:.2rem}"

      /* table */
      ".info-table{width:100%;border-collapse:collapse;font-size:.85rem}"
      ".info-table td{padding:.45rem .5rem;border-bottom:1px solid #1e1e1e;color:#c9d1d9}"
      ".info-table td:first-child{color:#8b949e;width:9rem}"

      /* badge */
      ".badge{display:inline-block;padding:.15rem .55rem;border-radius:.25rem;"
             "font-size:.75rem;font-weight:bold}"
      ".badge.green{background:#0d2a0d;color:#3fb950;border:1px solid #2a6a2a}"
      ".badge.red{background:#2a0d0d;color:#f85149;border:1px solid #6a2a2a}"
      ".badge.grey{background:#1e1e1e;color:#888;border:1px solid #333}"
    "</style>"
  "</head>"

  "<body x-data=\"hexaos()\" x-init=\"init()\">"

    /* topbar */
    "<div id=\"topbar\">"
      "<span class=\"logo\">" HX_SYSTEM_NAME "</span>"
      "<div class=\"meta\">"
        "<span>"
          "<span class=\"dot\" :class=\"online ? 'online' : 'offline'\"></span>"
          "<span x-text=\"online ? 'online' : 'offline'\"></span>"
        "</span>"
        "<span>uptime <span x-text=\"uptime\"></span></span>"
        "<span>v" HX_VERSION "</span>"
      "</div>"
    "</div>"

    /* layout */
    "<div id=\"layout\">"

      /* sidebar */
      "<nav id=\"sidebar\">"
        "<a :class=\"page==='dashboard' && 'active'\" @click=\"page='dashboard'\">Dashboard</a>"
        "<a :class=\"page==='network'   && 'active'\" @click=\"page='network'\">Network</a>"
        "<a :class=\"page==='storage'   && 'active'\" @click=\"page='storage'\">Storage</a>"
        "<a :class=\"page==='system'    && 'active'\" @click=\"page='system'\">System</a>"
        "<a :class=\"page==='console'   && 'active'\" @click=\"page='console';$nextTick(()=>consoleConnect())\">Console</a>"
      "</nav>"

      /* main content */
      "<div id=\"main\">"

        /* dashboard */
        "<div x-show=\"page==='dashboard'\">"
          "<h2>Dashboard</h2>"
          "<div class=\"cards\">"
            "<div class=\"card\">"
              "<div class=\"label\">Uptime</div>"
              "<div class=\"value\" x-text=\"uptime\">--</div>"
            "</div>"
            "<div class=\"card\">"
              "<div class=\"label\">Network</div>"
              "<div class=\"value\">"
                "<span class=\"badge\" :class=\"online ? 'green' : 'grey'\" x-text=\"online ? 'up' : 'down'\"></span>"
              "</div>"
            "</div>"
            "<div class=\"card\">"
              "<div class=\"label\">Board</div>"
              "<div class=\"value\" style=\"font-size:.95rem\">" HX_TARGET_NAME "</div>"
            "</div>"
          "</div>"
        "</div>"

        /* network */
        "<div x-show=\"page==='network'\" x-cloak>"
          "<h2>Network</h2>"
          "<table class=\"info-table\">"
            "<tr><td>Status</td><td>"
              "<span class=\"badge\" :class=\"online ? 'green' : 'grey'\" x-text=\"online ? 'connected' : 'disconnected'\"></span>"
            "</td></tr>"
            "<tr><td>IP</td><td x-text=\"ip || '—'\"></td></tr>"
            "<tr><td>RSSI</td><td x-text=\"rssi ? rssi + ' dBm' : '—'\"></td></tr>"
          "</table>"
        "</div>"

        /* storage */
        "<div x-show=\"page==='storage'\" x-cloak>"
          "<h2>Storage</h2>"
          "<table class=\"info-table\">"
            "<tr><td>LittleFS</td><td><span class=\"badge grey\">coming soon</span></td></tr>"
            "<tr><td>SD Card</td><td><span class=\"badge grey\">coming soon</span></td></tr>"
          "</table>"
        "</div>"

        /* system */
        "<div x-show=\"page==='system'\" x-cloak>"
          "<h2>System</h2>"
          "<table class=\"info-table\">"
            "<tr><td>Firmware</td><td>v" HX_VERSION "</td></tr>"
            "<tr><td>Board</td><td>" HX_TARGET_NAME "</td></tr>"
            "<tr><td>Build</td><td>" HX_BUILD_BOARD_ID "</td></tr>"
          "</table>"
        "</div>"

        /* console */
        "<div x-show=\"page==='console'\" x-cloak style=\"height:100%;display:flex;flex-direction:column\">"
          "<h2 style=\"flex-shrink:0;margin-bottom:1rem\">Console</h2>"
          "<div id=\"console-wrap\">"
            "<div id=\"console-out\" x-ref=\"consoleOut\">"
              "<template x-for=\"(line, i) in consoleLines\" :key=\"i\">"
                "<div :class=\"line.err ? 'line-err' : ''\" x-text=\"line.text\"></div>"
              "</template>"
            "</div>"
            "<div id=\"console-in\">"
              "<span>&#x276F;</span>"
              "<input type=\"text\" x-model=\"consoleInput\" autocomplete=\"off\" spellcheck=\"false\""
                     "@keydown.enter=\"consoleSend()\""
                     "@keydown.up.prevent=\"consoleHistoryUp()\""
                     "@keydown.down.prevent=\"consoleHistoryDown()\""
                     "placeholder=\"type command...\" x-ref=\"consoleInput\">"
            "</div>"
          "</div>"
        "</div>"

      "</div>" /* #main */
    "</div>" /* #layout */

    "<script>"
      "function hexaos(){"
        "return{"
          "page:'dashboard',"
          "online:false,"
          "uptime:'--',"
          "ip:null,"
          "rssi:null,"

          "consoleLines:[],"
          "consoleInput:'',"
          "consoleHistory:[],"
          "consoleHistoryIdx:-1,"
          "consoleWs:null,"

          "init(){"
            "this.poll();"
            "setInterval(()=>this.poll(),5000)"
          "},"

          "poll(){"
            "fetch('/api/status').then(r=>r.json()).then(d=>{"
              "this.online=d.net_connected;"
              "this.uptime=d.uptime;"
              "this.ip=d.ip;"
              "this.rssi=d.rssi"
            "}).catch(()=>{})"
          "},"

          "consoleConnect(){"
            "if(this.consoleWs&&this.consoleWs.readyState<2)return;"
            "const ws=new WebSocket('ws://'+location.host+'/ws/console');"
            "ws.onmessage=e=>{"
              "const lines=e.data.split('\\n');"
              "lines.forEach(l=>{"
                "if(l!=='')this.consoleLines.push({text:l,err:l.startsWith('ERR')||l.startsWith('error')});"
              "});"
              "this.$nextTick(()=>{"
                "const el=this.$refs.consoleOut;"
                "if(el)el.scrollTop=el.scrollHeight"
              "})"
            "};"
            "ws.onclose=()=>this.consoleLines.push({text:'[disconnected]',err:true});"
            "this.consoleWs=ws;"
            "this.$nextTick(()=>this.$refs.consoleInput&&this.$refs.consoleInput.focus())"
          "},"

          "consoleSend(){"
            "const cmd=this.consoleInput.trim();"
            "if(!cmd)return;"
            "if(this.consoleWs&&this.consoleWs.readyState===1){"
              "this.consoleLines.push({text:'> '+cmd,err:false});"
              "this.consoleHistory.unshift(cmd);"
              "if(this.consoleHistory.length>50)this.consoleHistory.pop();"
              "this.consoleHistoryIdx=-1;"
              "this.consoleWs.send(cmd);"
              "this.consoleInput=''"
            "}"
          "},"

          "consoleHistoryUp(){"
            "if(this.consoleHistory.length===0)return;"
            "this.consoleHistoryIdx=Math.min(this.consoleHistoryIdx+1,this.consoleHistory.length-1);"
            "this.consoleInput=this.consoleHistory[this.consoleHistoryIdx]"
          "},"

          "consoleHistoryDown(){"
            "if(this.consoleHistoryIdx<=0){this.consoleHistoryIdx=-1;this.consoleInput='';return;}"
            "this.consoleHistoryIdx--;"
            "this.consoleInput=this.consoleHistory[this.consoleHistoryIdx]"
          "}"
        "}"
      "}"
    "</script>"

  "</body>"
  "</html>";
