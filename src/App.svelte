<script>
  import { invoke } from '@tauri-apps/api/core'
  import { listen } from '@tauri-apps/api/event'
  import { getCurrentWindow } from '@tauri-apps/api/window'
  import { readText, writeText } from '@tauri-apps/plugin-clipboard-manager'
  import { onMount, tick } from 'svelte'

  const appWindow = getCurrentWindow()

  let versions = $state([])
  let version = $state('12.1')
  let alwaysOnTop = $state(false)
  let trayOnClose = $state(true)

  let search = $state('')
  let hex = $state('')
  let dec = $state('')
  let selected = $state(null)
  let flashing = $state(false)

  let allNames = $state([])
  let listEl = $state(null)
  let refreshing = $state(false)
  let animateIn = $state(false)

  let valueToName = new Map()
  let nameToValue = new Map()

  let filteredNames = $derived(
    allNames.filter((n) => n.toLowerCase().includes(search.trim().toLowerCase()))
  )

  onMount(async () => {
    listen('window-restored', () => {
      animateIn = false
      requestAnimationFrame(() => {
        animateIn = true
        setTimeout(() => (animateIn = false), 800)
      })
    })

    versions = await invoke('get_versions')

    const settings = await invoke('get_settings')
    version = settings.version
    alwaysOnTop = settings.alwaysOnTop
    trayOnClose = settings.trayOnClose
    if (!versions.includes(version)) version = '12.1'

    await loadOpcodes()
  })

  async function loadOpcodes() {
    const entries = await invoke('get_opcodes', { version })
    valueToName = new Map()
    nameToValue = new Map()
    for (const { value, name } of entries) {
      if (!valueToName.has(value)) valueToName.set(value, name)
      if (!nameToValue.has(name)) nameToValue.set(name, value)
    }
    allNames = [...nameToValue.keys()].sort()
    selected = null
    recheck()
    refreshing = false
    requestAnimationFrame(() => (refreshing = true))
  }

  function saveSettings() {
    invoke('save_settings', {
      settings: { version, alwaysOnTop, trayOnClose }
    }).catch(() => {})
  }

  function onVersionChange() {
    saveSettings()
    loadOpcodes()
  }

  async function onAlwaysOnTopChange() {
    await appWindow.setAlwaysOnTop(alwaysOnTop)
    saveSettings()
  }

  function onTrayOnCloseChange() {
    saveSettings()
  }

  function onHexInput() {
    const text = hex.trim()
    if (!text) {
      dec = ''
      selected = null
      return
    }
    const val = parseInt(text.replace(/^0x/i, ''), 16)
    if (Number.isNaN(val)) {
      selected = null
      return
    }
    dec = String(val)
    checkOpcode(val)
  }

  function onDecInput() {
    const text = dec.trim()
    if (!text) {
      hex = ''
      selected = null
      return
    }
    const val = Number(text)
    if (!Number.isInteger(val) || val < 0) {
      selected = null
      return
    }
    hex = '0x' + val.toString(16).toUpperCase()
    checkOpcode(val)
  }

  function checkOpcode(val) {
    const name = valueToName.get(val)
    if (name && filteredNames.includes(name)) {
      selected = name
      scrollToSelected()
      flashList()
    } else {
      selected = null
    }
  }

  function recheck() {
    const val = Number(dec.trim())
    if (Number.isInteger(val) && dec.trim()) checkOpcode(val)
  }

  function selectName(name) {
    selected = name
    const val = nameToValue.get(name)
    if (val !== undefined) {
      hex = '0x' + val.toString(16).toUpperCase()
      dec = String(val)
    }
    flashList()
  }

  async function scrollToSelected() {
    await tick()
    listEl
      ?.querySelector(`[data-name="${CSS.escape(selected)}"]`)
      ?.scrollIntoView({ block: 'nearest' })
  }

  function flashList() {
    flashing = false
    requestAnimationFrame(() => {
      flashing = true
      setTimeout(() => (flashing = false), 500)
    })
  }

  function clearAll() {
    hex = ''
    dec = ''
    selected = null
  }

  const copyHex = () => writeText(hex)
  const copyDec = () => writeText(dec)
  const copyOpcode = () => selected && writeText(selected)
  const pasteHex = async () => {
    hex = await readText()
    onHexInput()
  }
  const pasteDec = async () => {
    dec = await readText()
    onDecInput()
  }
</script>

<main class:animate-in={animateIn}>
  <h1>HexDec TC Converter</h1>

  <div class="top-controls">
    <div class="select-wrap">
      <select bind:value={version} onchange={onVersionChange}>
        {#each versions as v}
          <option value={v}>{v}</option>
        {/each}
      </select>
    </div>

    <div class="checks">
      <label class="switch">
        <input
          type="checkbox"
          bind:checked={alwaysOnTop}
          onchange={onAlwaysOnTopChange}
        />
        <span class="slider"></span>
        Always on Top
      </label>
      <label class="switch">
        <input
          type="checkbox"
          bind:checked={trayOnClose}
          onchange={onTrayOnCloseChange}
        />
        <span class="slider"></span>
        Tray on Exit
      </label>
    </div>
  </div>

  <div class="opcode-panel">
    <input
      class="search"
      type="text"
      placeholder="Search Opcode..."
      bind:value={search}
    />
    <button class="copy-opcode" onclick={copyOpcode}>Copy Opcode Name</button>
    <div class="list" class:flashing class:refreshing bind:this={listEl}>
      {#each filteredNames as name (name)}
        <div
          class="row"
          class:selected={selected === name}
          data-name={name}
          role="option"
          aria-selected={selected === name}
          tabindex="-1"
          onclick={() => selectName(name)}
          onkeydown={(e) => e.key === 'Enter' && selectName(name)}
        >
          {name}
        </div>
      {/each}
    </div>
  </div>

  <div class="field">
    <span class="label">Hexadecimal (0x...):</span>
    <div class="row-inputs">
      <input type="text" bind:value={hex} oninput={onHexInput} />
      <button class="small" onclick={pasteHex}>Paste</button>
      <button class="small" onclick={copyHex}>Copy</button>
    </div>
  </div>

  <div class="field">
    <span class="label">Decimal:</span>
    <div class="row-inputs">
      <input type="text" bind:value={dec} oninput={onDecInput} />
      <button class="small" onclick={pasteDec}>Paste</button>
      <button class="small" onclick={copyDec}>Copy</button>
    </div>
  </div>

  <button class="clear" onclick={clearAll}>Clear All</button>
</main>

<style>
  main {
    display: flex;
    flex-direction: column;
    height: 100%;
    padding: 20px;
    background:
      radial-gradient(ellipse 120% 60% at 50% -10%, rgba(0, 122, 204, 0.18), transparent 60%),
      radial-gradient(ellipse 80% 40% at 100% 110%, rgba(124, 58, 237, 0.1), transparent 60%),
      #1b1b1f;
  }

  @keyframes rise {
    from {
      opacity: 0;
      transform: translateY(12px) scale(0.98);
    }
    to {
      opacity: 1;
      transform: translateY(0) scale(1);
    }
  }

  main.animate-in > * {
    animation: rise 0.45s cubic-bezier(0.2, 0.7, 0.3, 1) both;
  }

  main.animate-in > *:nth-child(2) {
    animation-delay: 0.06s;
  }

  main.animate-in > *:nth-child(3) {
    animation-delay: 0.12s;
  }

  main.animate-in > *:nth-child(4) {
    animation-delay: 0.18s;
  }

  main.animate-in > *:nth-child(5) {
    animation-delay: 0.24s;
  }

  main.animate-in > *:nth-child(6) {
    animation-delay: 0.3s;
  }

  h1 {
    font-size: 20px;
    font-weight: 700;
    letter-spacing: 0.4px;
    text-align: center;
    margin: 0 0 16px;
    background: linear-gradient(90deg, #7cc4ff, #3a9bdc 50%, #9d7bff);
    -webkit-background-clip: text;
    background-clip: text;
    color: transparent;
    filter: drop-shadow(0 2px 10px rgba(58, 155, 220, 0.35));
  }

  .top-controls {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 15px;
  }

  .select-wrap {
    position: relative;
  }

  .select-wrap::after {
    content: '';
    position: absolute;
    right: 10px;
    top: 50%;
    width: 7px;
    height: 7px;
    border-right: 2px solid #8ab8e6;
    border-bottom: 2px solid #8ab8e6;
    transform: translateY(-70%) rotate(45deg);
    pointer-events: none;
  }

  select {
    appearance: none;
    background: rgba(255, 255, 255, 0.05);
    color: #fff;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 8px;
    padding: 6px 28px 6px 12px;
    font-size: 13px;
    cursor: pointer;
    transition: border-color 0.15s, background 0.15s;
  }

  select:hover,
  select:focus {
    border-color: #3a9bdc;
    background: rgba(58, 155, 220, 0.12);
    outline: none;
  }

  select option {
    background: #252528;
  }

  .checks {
    display: flex;
    gap: 14px;
    font-size: 12px;
    color: #cfcfd4;
  }

  .switch {
    display: flex;
    align-items: center;
    gap: 7px;
    cursor: pointer;
    user-select: none;
  }

  .switch input {
    display: none;
  }

  .slider {
    width: 30px;
    height: 17px;
    border-radius: 999px;
    background: rgba(255, 255, 255, 0.12);
    border: 1px solid rgba(255, 255, 255, 0.15);
    position: relative;
    transition: background 0.2s, border-color 0.2s;
    flex-shrink: 0;
  }

  .slider::after {
    content: '';
    position: absolute;
    top: 2px;
    left: 2px;
    width: 11px;
    height: 11px;
    border-radius: 50%;
    background: #d0d0d6;
    transition: transform 0.2s, background 0.2s;
  }

  .switch input:checked + .slider {
    background: linear-gradient(90deg, #2b7fc4, #4aa8e8);
    border-color: transparent;
    box-shadow: 0 0 8px rgba(58, 155, 220, 0.5);
  }

  .switch input:checked + .slider::after {
    transform: translateX(13px);
    background: #fff;
  }

  .opcode-panel {
    background: rgba(255, 255, 255, 0.045);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 10px;
    padding: 10px;
    margin-bottom: 15px;
    display: flex;
    flex-direction: column;
    gap: 6px;
    flex: 1;
    min-height: 140px;
    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.35);
  }

  input[type='text'] {
    background: rgba(0, 0, 0, 0.28);
    color: #fff;
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 8px;
    padding: 7px 9px;
    outline: none;
    min-width: 0;
    font-family: 'Cascadia Code', 'Consolas', monospace;
    font-size: 13px;
    transition: border-color 0.15s, box-shadow 0.15s, background 0.15s;
  }

  input[type='text']:focus {
    border-color: #3a9bdc;
    box-shadow: 0 0 0 3px rgba(58, 155, 220, 0.18);
    background: rgba(0, 0, 0, 0.38);
  }

  input[type='text']::selection {
    background: #3a7ebf;
  }

  input[type='text']::placeholder {
    color: #77777e;
  }

  .search {
    padding: 6px 9px;
  }

  button {
    background: rgba(255, 255, 255, 0.07);
    color: #e8e8ec;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 8px;
    padding: 8px 5px;
    cursor: pointer;
    font-size: 13px;
    transition: background 0.15s, border-color 0.15s, box-shadow 0.15s, transform 0.05s;
  }

  button:hover {
    background: rgba(58, 155, 220, 0.22);
    border-color: #3a9bdc;
    box-shadow: 0 0 10px rgba(58, 155, 220, 0.25);
  }

  button:active {
    background: rgba(0, 122, 204, 0.45);
    transform: scale(0.98);
  }

  .copy-opcode {
    padding: 6px;
  }

  .list {
    flex: 1;
    overflow-y: auto;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 2px;
  }

  .list.flashing {
    animation: flash 0.25s ease-in-out 2;
  }

  .list.refreshing {
    animation: listfade 0.35s ease;
  }

  @keyframes listfade {
    from {
      opacity: 0;
      transform: translateY(5px);
    }
    to {
      opacity: 1;
      transform: translateY(0);
    }
  }

  @keyframes flash {
    50% {
      border-color: #3a9bdc;
      box-shadow: inset 0 0 12px rgba(58, 155, 220, 0.35);
    }
  }

  .row {
    padding: 4px 8px;
    font-size: 12.5px;
    font-family: 'Cascadia Code', 'Consolas', monospace;
    border-radius: 5px;
    cursor: pointer;
    white-space: nowrap;
    color: #d4d4da;
    transition: background 0.1s;
  }

  .row:hover {
    background: rgba(255, 255, 255, 0.07);
    color: #fff;
  }

  .row.selected {
    background: linear-gradient(90deg, #007acc, #3a9bdc);
    color: #fff;
    box-shadow: 0 2px 8px rgba(0, 122, 204, 0.4);
  }

  .field {
    margin-bottom: 10px;
  }

  .label {
    display: block;
    font-size: 10.5px;
    font-weight: 600;
    letter-spacing: 0.6px;
    text-transform: uppercase;
    color: #8f8f98;
    margin-bottom: 4px;
  }

  .row-inputs {
    display: flex;
    gap: 6px;
  }

  .row-inputs input {
    flex: 1;
  }

  button.small {
    width: 55px;
    padding: 5px;
    font-size: 12px;
  }

  .clear {
    margin: 4px 0 0;
    padding: 10px 15px;
    font-weight: 600;
    letter-spacing: 0.3px;
    background: rgba(255, 255, 255, 0.06);
  }

  .clear:hover {
    background: rgba(220, 80, 80, 0.18);
    border-color: rgba(220, 90, 90, 0.6);
    box-shadow: 0 0 10px rgba(220, 80, 80, 0.2);
  }

  .list::-webkit-scrollbar {
    width: 10px;
    height: 10px;
  }

  .list::-webkit-scrollbar-corner {
    background: transparent;
  }

  .list::-webkit-scrollbar-track {
    background: transparent;
  }

  .list::-webkit-scrollbar-thumb {
    background: rgba(255, 255, 255, 0.14);
    border-radius: 999px;
    border: 2px solid transparent;
    background-clip: content-box;
  }

  .list::-webkit-scrollbar-thumb:hover {
    background: rgba(58, 155, 220, 0.5);
    background-clip: content-box;
  }
</style>
