#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

const char WEB_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OENXC-10 設定</title>
    <style>
        :root {
            --bg-color: #121212;
            --surface-color: #1e1e1e;
            --primary-color: #00e5ff;
            --text-main: #ffffff;
            --text-muted: #a0a0a0;
            --border-color: #333333;
        }
        * { box-sizing: border-box; font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }
        body {
            background-color: var(--bg-color); color: var(--text-main);
            margin: 0; padding: 20px;
            display: flex; justify-content: center;
        }
        .container {
            width: 100%; max-width: 600px;
            background: var(--surface-color);
            padding: 30px; border-radius: 16px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            border: 1px solid var(--border-color);
        }
        h1 { text-align: center; color: var(--primary-color); margin-top: 0; }
        .section {
            margin-bottom: 25px; padding-bottom: 20px;
            border-bottom: 1px solid var(--border-color);
        }
        .section:last-child { border-bottom: none; }
        .section-title { font-size: 1.2em; font-weight: 600; margin-bottom: 15px; color: var(--primary-color); }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-size: 0.9em; color: var(--text-muted); }
        input[type="text"], input[type="password"], input[type="number"], input[type="time"], select {
            width: 100%; padding: 10px; border-radius: 8px;
            background: #2a2a2a; border: 1px solid var(--border-color);
            color: white; font-size: 1em;
            transition: border-color 0.3s;
        }
        input:focus, select:focus { outline: none; border-color: var(--primary-color); }
        .checkbox-group { display: flex; align-items: center; gap: 10px; }
        .checkbox-group input { width: 20px; height: 20px; accent-color: var(--primary-color); }
        button {
            width: 100%; padding: 12px; border-radius: 8px;
            background: var(--primary-color); color: #000;
            border: none; font-size: 1.1em; font-weight: bold;
            cursor: pointer; transition: transform 0.1s, filter 0.3s;
        }
        button:hover { filter: brightness(1.1); }
        button:active { transform: scale(0.98); }
        button.secondary { background: #333; color: white; margin-top: 10px; }
        .info-box { background: #2a2a2a; padding: 15px; border-radius: 8px; margin-bottom: 15px; }
        .hidden { display: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>OENXC-10 Settings</h1>
        
        <div class="info-box">
            <div><strong>Uptime (Cumulative): </strong><span id="cumTime">Loading...</span></div>
        </div>

        <div class="section">
            <div class="section-title">📊 System Status (Sensors)</div>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px;">
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">Temperature</div>
                    <div style="font-size: 1.2em; font-weight: bold; color: var(--primary-color);" id="monTemp">-- °C</div>
                </div>
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">Humidity</div>
                    <div style="font-size: 1.2em; font-weight: bold; color: var(--primary-color);" id="monHum">-- %</div>
                </div>
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">Pressure</div>
                    <div style="font-size: 1.2em; font-weight: bold; color: var(--primary-color);" id="monPres">-- hPa</div>
                </div>
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">Ambient Light (ADC)</div>
                    <div style="font-size: 1.2em; font-weight: bold; color: var(--primary-color);" id="monAdc">--</div>
                </div>
            </div>
            
            <div class="section-title" style="margin-top: 15px; font-size: 1.0em;">📡 Network Status</div>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px;">
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">WiFi Connection</div>
                    <div style="font-size: 1.0em; font-weight: bold; color: var(--primary-color); word-break: break-all;" id="monWifi">--</div>
                </div>
                <div class="info-box" style="margin: 0; text-align: center;">
                    <div style="font-size: 0.8em; color: var(--text-muted);">Last NTP Sync</div>
                    <div style="font-size: 1.0em; font-weight: bold; color: var(--primary-color);" id="monNtpSync">--</div>
                </div>
            </div>
            <button class="secondary" id="refreshStatusBtn" style="padding: 8px; font-size: 0.9em;">Refresh Status</button>
        </div>

        <div class="section">
            <div class="section-title">🕒 Time Synchronization</div>
            <button class="secondary" id="syncTimeBtn" style="margin-bottom: 15px;">Sync Time with this Device</button>
            <div style="background: #2a2a2a; padding: 15px; border-radius: 8px; margin-bottom: 10px;">
                <div style="font-size: 0.9em; color: var(--text-muted); margin-bottom: 10px;">Set Date & Time Manually</div>
                <div style="display: flex; gap: 10px; align-items: center; justify-content: center; margin-bottom: 10px;">
                    <input type="number" id="manualYear" min="2020" max="2099" placeholder="YYYY" style="width: 80px; text-align: center; font-size: 1.2em;"> /
                    <input type="number" id="manualMonth" min="1" max="12" placeholder="MM" style="width: 60px; text-align: center; font-size: 1.2em;"> /
                    <input type="number" id="manualDay" min="1" max="31" placeholder="DD" style="width: 60px; text-align: center; font-size: 1.2em;">
                </div>
                <div style="display: flex; gap: 10px; align-items: center; justify-content: center; margin-bottom: 10px;">
                    <input type="number" id="manualHour" min="0" max="23" placeholder="HH" style="width: 60px; text-align: center; font-size: 1.2em;"> :
                    <input type="number" id="manualMin" min="0" max="59" placeholder="MM" style="width: 60px; text-align: center; font-size: 1.2em;"> :
                    <input type="number" id="manualSec" min="0" max="59" placeholder="SS" style="width: 60px; text-align: center; font-size: 1.2em;">
                </div>
                <button class="secondary" id="setManualTimeBtn">Set Custom Time</button>
            </div>
            <p id="syncStatus" style="color: var(--primary-color); font-size: 0.9em; margin-top: 10px;"></p>
        </div>

        <form id="settingsForm">
            <div class="section">
                <div class="section-title">🌐 WiFi Client Settings</div>
                <div class="form-group">
                    <label>SSID</label>
                    <input type="text" id="ssid" name="ssid">
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" id="pass" name="pass">
                </div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="dhcp" name="dhcp">
                    <label for="dhcp" style="margin:0;">Use DHCP</label>
                </div>
                <div id="staticIpGroup" class="hidden">
                    <div class="form-group">
                        <label>Static IP</label>
                        <input type="text" id="ip" name="ip" placeholder="192.168.1.100">
                    </div>
                    <div class="form-group">
                        <label>Gateway</label>
                        <input type="text" id="gw" name="gw" placeholder="192.168.1.1">
                    </div>
                    <div class="form-group">
                        <label>Subnet Mask</label>
                        <input type="text" id="mask" name="mask" placeholder="255.255.255.0">
                    </div>
                </div>
                <div class="form-group">
                    <label>NTP Server</label>
                    <input type="text" id="ntp" name="ntp" placeholder="pool.ntp.org">
                </div>
            </div>

            <div class="section" id="demoControlSection" style="display: none;">
                <div class="section-title">🎲 Random Demo Control</div>
                <button type="button" class="secondary" id="triggerDemoBtn">Trigger Random Shuffle</button>
                <p id="demoStatus" style="color: var(--primary-color); font-size: 0.9em; margin-top: 10px;"></p>
            </div>

            <div class="section">
                <div class="section-title">✨ Display Settings</div>
                <div class="form-group">
                    <label>Operation Mode</label>
                    <select id="appMode" name="appMode">
                        <option value="0">Clock Mode</option>
                        <option value="1">Random Demo Mode</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Transition Mode</label>
                    <select id="dispMode" name="dispMode">
                        <option value="0">Normal (Instant)</option>
                        <option value="1">Fade (Dim/Bright)</option>
                        <option value="2">Crossfade</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Nixie Current (DAC 0-255)</label>
                    <input type="number" id="current" name="current" min="0" max="255">
                </div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="brightAuto" name="brightAuto">
                    <label for="brightAuto" style="margin:0;">Auto Brightness (Ambient Light)</label>
                </div>
                <div id="brightAutoGroup" class="hidden" style="margin-left: 20px; border-left: 2px solid #ccc; padding-left: 10px;">
                    <div class="form-group">
                        <label>ADC Min Threshold (Dark)</label>
                        <input type="number" id="adcMin" name="adcMin" min="0" max="4095">
                    </div>
                    <div class="form-group">
                        <label>ADC Max Threshold (Bright)</label>
                        <input type="number" id="adcMax" name="adcMax" min="0" max="4095">
                    </div>
                </div>
                <div class="form-group" id="brightLvGroup">
                    <label>Manual Brightness Level (0-255)</label>
                    <input type="number" id="brightLv" name="brightLv" min="0" max="255">
                </div>
            </div>

            <div class="section">
                <div class="section-title">🔋 Power Saving Mode</div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="psEnable" name="psEnable">
                    <label for="psEnable" style="margin:0;">Enable Auto ON/OFF</label>
                </div>
                <div id="psTimeGroup" class="hidden">
                    <div class="form-group">
                        <label>Turn ON Time</label>
                        <input type="time" id="psOn" name="psOn">
                    </div>
                    <div class="form-group">
                        <label>Turn OFF Time</label>
                        <input type="time" id="psOff" name="psOff">
                    </div>
                </div>
            </div>

            <button type="submit" id="saveBtn">Save Settings</button>
        </form>
    </div>

    <script>
        // Toggle UI logic
        const dhcpCheck = document.getElementById('dhcp');
        const staticIpGroup = document.getElementById('staticIpGroup');
        dhcpCheck.addEventListener('change', (e) => {
            staticIpGroup.classList.toggle('hidden', e.target.checked);
        });

        const brightAutoCheck = document.getElementById('brightAuto');
        const brightLvGroup = document.getElementById('brightLvGroup');
        const brightAutoGroup = document.getElementById('brightAutoGroup');
        brightAutoCheck.addEventListener('change', (e) => {
            brightLvGroup.classList.toggle('hidden', e.target.checked);
            brightAutoGroup.classList.toggle('hidden', !e.target.checked);
        });

        const psEnableCheck = document.getElementById('psEnable');
        const psTimeGroup = document.getElementById('psTimeGroup');
        psEnableCheck.addEventListener('change', (e) => {
            psTimeGroup.classList.toggle('hidden', !e.target.checked);
        });

        // Load settings
        fetch('/api/settings').then(res => res.json()).then(data => {
            document.getElementById('ssid').value = data.ssid || '';
            document.getElementById('pass').value = data.pass || '';
            dhcpCheck.checked = data.dhcp;
            document.getElementById('ip').value = data.ip || '';
            document.getElementById('gw').value = data.gw || '';
            document.getElementById('mask').value = data.mask || '';
            document.getElementById('ntp').value = data.ntp || '';
            document.getElementById('appMode').value = data.appMode || 0;
            document.getElementById('dispMode').value = data.dispMode || 0;
            document.getElementById('current').value = data.current || 23;
            brightAutoCheck.checked = data.brightAuto;

            // Toggle demo control section based on appMode
            const demoSection = document.getElementById('demoControlSection');
            demoSection.style.display = (data.appMode == 1) ? 'block' : 'none';
            document.getElementById('appMode').addEventListener('change', (e) => {
                demoSection.style.display = (e.target.value == 1) ? 'block' : 'none';
            });
            document.getElementById('brightLv').value = data.brightLv || 255;
            document.getElementById('adcMin').value = data.adcMin || 500;
            document.getElementById('adcMax').value = data.adcMax || 2500;
            psEnableCheck.checked = data.psEnable;
            
            // Format times to HH:MM
            const pad = (n) => n.toString().padStart(2, '0');
            document.getElementById('psOn').value = `${pad(data.psOnHr)}:${pad(data.psOnMin)}`;
            document.getElementById('psOff').value = `${pad(data.psOffHr)}:${pad(data.psOffMin)}`;

            // Trigger events to set visibility
            dhcpCheck.dispatchEvent(new Event('change'));
            brightAutoCheck.dispatchEvent(new Event('change'));
            psEnableCheck.dispatchEvent(new Event('change'));
            
            // Format cumulative time
            const cumSec = data.cumSec || 0;
            const hours = Math.floor(cumSec / 3600);
            document.getElementById('cumTime').innerText = `${hours} Hours`;
        });
        
        // Load System Status
        function loadSystemStatus() {
            fetch('/api/status').then(res => res.json()).then(data => {
                document.getElementById('monTemp').innerText = data.temp !== undefined ? (data.temp.toFixed(1) + ' °C') : '-- °C';
                document.getElementById('monHum').innerText = data.hum !== undefined ? (data.hum.toFixed(1) + ' %') : '-- %';
                document.getElementById('monPres').innerText = data.pres !== undefined ? (data.pres.toFixed(1) + ' hPa') : '-- hPa';
                document.getElementById('monAdc').innerText = data.adc !== undefined ? data.adc : '--';
                document.getElementById('monWifi').innerText = data.wifi !== undefined ? data.wifi : '--';
                document.getElementById('monNtpSync').innerText = data.ntpSync !== undefined ? data.ntpSync : '--';
            }).catch(e => console.error("Error loading status"));
        }
        
        // Initial load and manual refresh
        loadSystemStatus();
        document.getElementById('refreshStatusBtn').addEventListener('click', (e) => {
            e.preventDefault();
            loadSystemStatus();
        });
        
        // Auto refresh status every 5 seconds
        setInterval(loadSystemStatus, 5000);

        // Save settings
        document.getElementById('settingsForm').addEventListener('submit', (e) => {
            e.preventDefault();
            const btn = document.getElementById('saveBtn');
            btn.innerText = 'Saving...';
            btn.disabled = true;

            const payload = {
                ssid: document.getElementById('ssid').value,
                pass: document.getElementById('pass').value,
                dhcp: dhcpCheck.checked,
                ip: document.getElementById('ip').value,
                gw: document.getElementById('gw').value,
                mask: document.getElementById('mask').value,
                ntp: document.getElementById('ntp').value,
                appMode: parseInt(document.getElementById('appMode').value),
                dispMode: parseInt(document.getElementById('dispMode').value),
                current: parseInt(document.getElementById('current').value),
                brightAuto: brightAutoCheck.checked,
                brightLv: parseInt(document.getElementById('brightLv').value),
                adcMin: parseInt(document.getElementById('adcMin').value),
                adcMax: parseInt(document.getElementById('adcMax').value),
                psEnable: psEnableCheck.checked,
                psOnHr: parseInt(document.getElementById('psOn').value.split(':')[0] || 0),
                psOnMin: parseInt(document.getElementById('psOn').value.split(':')[1] || 0),
                psOffHr: parseInt(document.getElementById('psOff').value.split(':')[0] || 0),
                psOffMin: parseInt(document.getElementById('psOff').value.split(':')[1] || 0)
            };

            fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            }).then(() => {
                btn.innerText = 'Saved Successfully!';
                setTimeout(() => { btn.innerText = 'Save Settings'; btn.disabled = false; }, 2000);
            }).catch(() => {
                btn.innerText = 'Error Saving';
                setTimeout(() => { btn.innerText = 'Save Settings'; btn.disabled = false; }, 2000);
            });
        });

        // Sync Time function
        document.getElementById('setManualTimeBtn').addEventListener('click', () => {
            const yr = parseInt(document.getElementById('manualYear').value);
            const mo = parseInt(document.getElementById('manualMonth').value);
            const dy = parseInt(document.getElementById('manualDay').value);
            const h = parseInt(document.getElementById('manualHour').value);
            const m = parseInt(document.getElementById('manualMin').value);
            const s = parseInt(document.getElementById('manualSec').value);
            if(isNaN(yr) || isNaN(mo) || isNaN(dy) || isNaN(h) || isNaN(m) || isNaN(s)) {
                alert("Please enter valid date and time values");
                return;
            }
            
            const btn = document.getElementById('setManualTimeBtn');
            btn.innerText = 'Syncing...';
            btn.disabled = true;

            const now = new Date(yr, mo - 1, dy);
            const payload = {
                year: yr,
                month: mo,
                day: dy,
                hour: h,
                min: m,
                sec: s,
                week: now.getDay()
            };

            fetch('/api/time', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            }).then(() => {
                document.getElementById('syncStatus').innerText = 'Manual Time Set Successfully!';
                btn.innerText = 'Set Custom Time';
                btn.disabled = false;
            }).catch(() => {
                document.getElementById('syncStatus').innerText = 'Error setting manual time';
                btn.innerText = 'Set Custom Time';
                btn.disabled = false;
            });
        });

        document.getElementById('syncTimeBtn').addEventListener('click', () => {
            const btn = document.getElementById('syncTimeBtn');
            btn.innerText = 'Syncing...';
            btn.disabled = true;

            // Wait for the exact second boundary to send the request
            const now = new Date();
            const delay = 1000 - now.getMilliseconds();
            
            setTimeout(() => {
                const exactNow = new Date();
                const payload = {
                    year: exactNow.getFullYear(),
                    month: exactNow.getMonth() + 1,
                    day: exactNow.getDate(),
                    hour: exactNow.getHours(),
                    min: exactNow.getMinutes(),
                    sec: exactNow.getSeconds(),
                    week: exactNow.getDay()
                };
                
                fetch('/api/time', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                }).then(res => {
                    if(res.ok) document.getElementById('syncStatus').innerText = "Time synchronized successfully!";
                    else document.getElementById('syncStatus').innerText = "Failed to sync time.";
                    btn.innerText = 'Sync Time with this Device';
                    btn.disabled = false;
                });
            }, delay);
        });

        // Demo Trigger function
        document.getElementById('triggerDemoBtn').addEventListener('click', () => {
            const btn = document.getElementById('triggerDemoBtn');
            btn.innerText = 'Triggering...';
            btn.disabled = true;

            fetch('/api/demo_shuffle', { method: 'POST' })
            .then(() => {
                document.getElementById('demoStatus').innerText = 'Shuffle Triggered!';
                setTimeout(() => document.getElementById('demoStatus').innerText = '', 3000);
            })
            .catch(() => {
                document.getElementById('demoStatus').innerText = 'Error triggering shuffle';
            })
            .finally(() => {
                btn.innerText = 'Trigger Random Shuffle';
                btn.disabled = false;
            });
        });

        // Auto sync time if it's the first visit (optional UX)
        // setTimeout(() => document.getElementById('syncTimeBtn').click(), 1000);
    </script>
</body>
</html>
)=====";

#endif // WEB_ASSETS_H
