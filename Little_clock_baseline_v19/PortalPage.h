#ifndef PORTAL_PAGE_H
#define PORTAL_PAGE_H

// ============================================================
//  PORTAL HEADER
// ============================================================

const char PORTAL_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Little Clock Setup</title>

  <style>
    body{
      font-family:Arial,Helvetica,sans-serif;
      background:#f0f0f0;
      margin:0;
      padding:0;
    }

    .card{
      max-width:460px;
      margin:14px auto;
      background:#fff;
      padding:20px;
      border-radius:10px;
      box-shadow:0 2px 6px rgba(0,0,0,0.18);
    }

    h2,h3{
      margin-top:0;
      text-align:center;
    }

    label{
      font-size:16px;
      display:block;
      margin-bottom:6px;
    }

    input,
    button{
      width:100%;
      padding:12px;
      margin:8px 0 16px 0;
      font-size:17px;
      box-sizing:border-box;
      border-radius:6px;
    }

    button{
      background:#0078d7;
      color:#fff;
      border:none;
      font-weight:bold;
    }

    button:active{opacity:0.85;}

    .section{
      margin-bottom:20px;
    }

    .ssid-list button{
      width:100%;
      padding:10px;
      margin-bottom:8px;
      font-size:15px;
      text-align:left;
      background:#eaeaea;
      color:#000;
      border:1px solid #bbb;
      border-radius:6px;
    }

    .toggle-group{
      margin:6px 0 14px 0;
    }

    .toggle-group label{
      display:inline-block;
      margin-right:18px;
      font-size:15px;
    }

    /* ---------- Password eye ---------- */
    .pw-wrap{
      position:relative;
    }
    .pw-wrap input{
      padding-right:42px;
    }
    .pw-eye{
      position:absolute;
      right:12px;
      top:50%;
      transform:translateY(-50%);
      cursor:pointer;
      user-select:none;
    }

    /* ---------- Grey-out when hex mode active ---------- */
    .fmt-disabled{
      opacity:0.35;
      pointer-events:none;
    }
  </style>

  <script>
    function setSSID(name){
      document.getElementById('ssid').value = name;
    }

    function togglePW(){
      var p      = document.getElementById('pass');
      var open   = document.getElementById('eyeOpen');
      var closed = document.getElementById('eyeClosed');

      var show = (p.type === "password");
      p.type = show ? "text" : "password";

      open.style.display   = show ? "inline" : "none";
      closed.style.display = show ? "none"  : "inline";
    }

    function updateFmtLock(){
      var hex = document.querySelector(
        "input[name='time_mode'][value='1']"
      );
      var fmt = document.getElementById('fmtGroup');

      if (hex.checked)
        fmt.classList.add('fmt-disabled');
      else
        fmt.classList.remove('fmt-disabled');
    }

    document.addEventListener('DOMContentLoaded', function(){
      var modes = document.querySelectorAll(
        "input[name='time_mode']"
      );

      for (var i = 0; i < modes.length; i++){
        modes[i].addEventListener('change', updateFmtLock);
      }

      updateFmtLock();
    });
  </script>
</head>

<body>
)rawliteral";

// ============================================================
//  PORTAL BODY TOP
// ============================================================

const char PORTAL_BODY_TOP[] PROGMEM = R"rawliteral(
<div class='card'>
  <h2>Little Clock Setup</h2>

  <form action='/save' method='POST'>

    <div class='section'>
      <h3>WiFi</h3>

      <label for='ssid'>SSID</label>
      <input id='ssid' name='ssid' placeholder='Network name'>

      <label for='pass'>Password</label>

      <div class='pw-wrap'>
        <input id='pass' name='pass'
               type='password'
               placeholder='Password'>

        <span class='pw-eye' onclick='togglePW()'>

          <svg id='eyeClosed'
               xmlns='http://www.w3.org/2000/svg'
               width='24'
               height='24'
               fill='none'
               stroke='#555'
               stroke-width='2'
               viewBox='0 0 24 24'>
            <path d='M1 12s4-7 11-7 11 7 11 7-4 7-11 7S1 12 1 12z'/>
            <line x1='3' y1='3' x2='21' y2='21'/>
          </svg>

          <svg id='eyeOpen'
               xmlns='http://www.w3.org/2000/svg'
               width='24'
               height='24'
               fill='none'
               stroke='#555'
               stroke-width='2'
               viewBox='0 0 24 24'
               style='display:none;'>
            <path d='M1 12s4-7 11-7 11 7 11 7-4 7-11 7S1 12 1 12z'/>
            <circle cx='12' cy='12' r='3'/>
          </svg>

        </span>
      </div>
    </div>

    <div class='section'>
      <h3>Available Networks</h3>
      <div class='ssid-list'>
)rawliteral";

// ============================================================
//  PORTAL BODY BOTTOM
// ============================================================

const char PORTAL_BODY_BOTTOM[] PROGMEM = R"rawliteral(
      </div>
    </div>

    <button type='submit'>Save &amp; Reboot</button>
  </form>
</div>
</body>
</html>
)rawliteral";

#endif
