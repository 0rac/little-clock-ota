#ifndef INFO_PORTAL_PAGE_H
#define INFO_PORTAL_PAGE_H

// ============================================================
//  INFO / UPDATE PORTAL — UI ONLY
// ============================================================

const char INFO_PORTAL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Little Clock Info</title>

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

    /* ---------- Tables ---------- */
    table{
      width:100%;
      border-collapse:collapse;
      font-size:15px;
    }
    td{
      padding:8px 6px;
      border-bottom:1px solid #eee;
    }
    td.key{
      color:#555;
      width:40%;
    }

    /* ---------- Buttons ---------- */
    button{
      width:100%;
      height:48px;
      padding:10px;
      font-size:15px;
      box-sizing:border-box;
      border-radius:6px;
      background:#0078d7;
      color:#fff;
      border:none;
      font-weight:bold;
    }
    button:active{opacity:0.85;}

    /* ---------- Secondary button ---------- */
    .exit-btn{
      background:#f5f5f5;
      color:#333;
      border:1px solid #ccc;
      font-weight:normal;
    }
    .exit-btn:active{background:#e8e8e8;}

    /* ---------- Split button row ---------- */
    .btn-row{
      display:flex;
      gap:10px;
      margin-top:16px;
    }
    .btn-row form{
      flex:1 1 0;
      margin:0;
    }
    .btn-row button{
      width:100%;
      height:48px;
      white-space:nowrap;
      display:flex;
      align-items:center;
      justify-content:center;
    }

    /* ---------- Notes ---------- */
    .note{
      margin-top:14px;
      font-size:14px;
      color:#444;
      text-align:center;
    }

    /* ---------- Options ---------- */
    .option{
      padding:8px 0;
      border-bottom:1px solid #eee;
    }
    .option:last-of-type{
      border-bottom:none;
    }
    .option label{
      font-size:15px;
    }
    .option.disabled{
      opacity:0.4;
    }

    /* ---------- Tabs ---------- */
    .tabs{
      display:flex;
      margin-bottom:16px;
      border-bottom:1px solid #ddd;
    }
    .tab{
      flex:1;
      padding:10px;
      text-align:center;
      cursor:pointer;
      font-weight:bold;
      color:#555;
    }
    .tab.active{
      color:#000;
      border-bottom:3px solid #0078d7;
    }
    .tabcontent{
      display:none;
    }
    .tabcontent.active{
      display:block;
    }
  </style>

  <script>
    function showTab(id){
      var tabs  = document.getElementsByClassName('tab');
      var pages = document.getElementsByClassName('tabcontent');

      for(var i=0;i<tabs.length;i++){
        tabs[i].classList.remove('active');
        pages[i].classList.remove('active');
      }

      document.getElementById('tab_' + id).classList.add('active');
      document.getElementById(id).classList.add('active');
    }
  </script>
</head>

<body>
<div class='card'>

  <h2>Little Clock</h2>

  <div class='tabs'>
    <div class='tab active' id='tab_info' onclick="showTab('info')">Info</div>
    <div class='tab' id='tab_options' onclick="showTab('options')">Options</div>
  </div>

  <!-- ================= INFO TAB ================= -->
  <div id='info' class='tabcontent active'>

    <table>
      <tr><td class='key'>WiFi SSID</td><td>%SSID%</td></tr>
      <tr><td class='key'>IP Address</td><td>%IP%</td></tr>
      <tr><td class='key'>MAC Address</td><td>%MAC%</td></tr>
      <tr><td class='key'>Firmware</td><td>%VERSION%</td></tr>
      <tr><td class='key'>Build</td><td>%BUILD%</td></tr>
      <tr><td class='key'>Serial Number</td><td>%SERIAL%</td></tr>
    </table>

    <div class='btn-row'>
      <form action='/check' method='POST'>
        <button type='submit'>Check for Update</button>
      </form>

      <form action='/exit' method='POST'>
        <button type='submit' class='exit-btn'>Exit to Clock</button>
      </form>
    </div>

    <div class='note'>
      Update system will check GitHub for a newer version.
    </div>
  </div>

  <!-- ================= OPTIONS TAB ================= -->
  <div id='options' class='tabcontent'>

    <form action='/info/options_save' method='POST'>

      <div class='option'>
        <label>
          <input type='radio' name='time_mode' value='12' %TIME12_CHECKED%>
          12-Hour Time
        </label>
      </div>

      <div class='option'>
        <label>
          <input type='radio' name='time_mode' value='24' %TIME24_CHECKED%>
          24-Hour Time
        </label>
      </div>

      <div class='option' id='hex_option'>
        <label>
          <input type='radio' name='time_mode' value='hex' %TIMEHEX_CHECKED%>
          Hexadecimal Time
        </label>
      </div>

      <div class='option' id='blink_colon_option'>
        <label>
          <input type='checkbox' name='blink_colon' %BLINK_CHECKED%>
          Blink Colon
        </label>
      </div>

      <div class='option' id='wifi_option'>
        <label>
          <input type='checkbox' name='wifi_bar' %WIFIBAR_CHECKED%>
          WiFi Signal Bar
        </label>
      </div>

      <div class='option'>
        <label>Font Style</label>

        <select name='font' id='font_select'
                style='width:100%; height:40px; margin-top:6px;'>
          <option value='-1' %FONT_DEFAULT_SELECTED%>Default</option>
          <option value='0'  %FONT_BLOCKY_SELECTED%>Blocky</option>
          <option value='1'  %FONT_BOLD_SELECTED%>Bold</option>
        </select>
      </div>

      <div class='btn-row'>
        <button type='submit'>Save &amp; Reboot</button>

        <button type='submit' formaction='/exit' class='exit-btn'>
          Exit to Clock
        </button>
      </div>
    </form>

    <div class='note'>
      Changes apply after reboot.
    </div>
  </div>
</div>

<!-- ======================================================== -->
<!--  CLIENT-SIDE LOGIC                                       -->
<!-- ======================================================== -->
<script>
function updateWifiForFont()
{
  const fontSel = document.getElementById('font_select');

  // --- WiFi option ---
  const wifiOpt = document.getElementById('wifi_option');
  const wifiChk = wifiOpt.querySelector('input');

  // Blocky (0) or Bold (1) → WiFi bar forced OFF
  if (fontSel.value === '0' || fontSel.value === '1')
  {
    wifiChk.checked  = false;
    wifiChk.disabled = true;
    wifiOpt.classList.add('disabled');
  }
  else
  {
    wifiChk.disabled = false;
    wifiOpt.classList.remove('disabled');
  }

  // --- Blink Colon option ---
  const colonOpt = document.getElementById('blink_colon_option');
  const colonChk = colonOpt.querySelector('input');

  // Blocky (0) → Blink colon disabled
  if (fontSel.value === '0')
  {
    colonChk.disabled = true;
    colonOpt.classList.add('disabled');
  }
  else
  {
    colonChk.disabled = false;
    colonOpt.classList.remove('disabled');
  }

  // --- Hex Time option ---
  const hexOpt = document.getElementById('hex_option');
  const hexChk = hexOpt.querySelector('input');

  // Bold (1) → Hex disabled (not supported)
  if (fontSel.value === '1')
  {
    hexChk.disabled = true;
    hexOpt.classList.add('disabled');
  }
  else
  {
    hexChk.disabled = false;
    hexOpt.classList.remove('disabled');
  }
}
  document.addEventListener('DOMContentLoaded', function () {
    updateWifiForFont();
    document.getElementById('font_select')
            .addEventListener('change', updateWifiForFont);
  });
</script>

</body>
</html>
)rawliteral";

// ============================================================
//  UPDATE CONFIRMATION PAGE — UI ONLY
// ============================================================

const char INFO_UPDATE_CONFIRM_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Firmware Update</title>

  <style>
    body{
      font-family:Arial,Helvetica,sans-serif;
      background:#f0f0f0;
      margin:0;
    }
    .card{
      max-width:460px;
      margin:40px auto;
      background:#fff;
      padding:24px;
      border-radius:10px;
      box-shadow:0 2px 6px rgba(0,0,0,0.18);
      text-align:center;
    }
    h2{margin-top:0;}
    .note{
      margin:18px 0;
      font-size:15px;
      color:#444;
    }
    .btn-row{
      display:flex;
      gap:10px;
      margin-top:20px;
    }
    .btn-row form{
      flex:1;
      margin:0;
    }
    button{
      width:100%;
      height:48px;
      font-size:15px;
      border-radius:6px;
      background:#0078d7;
      color:#fff;
      border:none;
      font-weight:bold;
    }
    button:active{opacity:0.85;}
    .exit-btn{
      background:#f5f5f5;
      color:#333;
      border:1px solid #ccc;
      font-weight:normal;
    }
  </style>
</head>

<body>
<div class="card">
  <h2>Firmware Update</h2>

  <div class="note">
    Updating to version <strong>%NEW_VERSION%</strong><br><br>
    The display may flicker during the update.<br>
    Do not power off the device.
  </div>

  <div class="btn-row">
    <form action="/update_start" method="POST">
      <button type="submit">Start Update</button>
    </form>

    <form action="/" method="GET">
      <button type="submit" class="exit-btn">Cancel</button>
    </form>
  </div>
</div>
</body>
</html>
)rawliteral";

#endif
