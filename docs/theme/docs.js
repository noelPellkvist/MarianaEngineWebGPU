// Mariana Engine documentation: search, theme, navigation, copy buttons, image zoom.
(function () {
  const root = document.body.dataset.root || '';

  // ---- Theme --------------------------------------------------------------------------------
  const themeButton = document.querySelector('.theme');
  themeButton && themeButton.addEventListener('click', () => {
    const next = document.documentElement.dataset.theme === 'light' ? 'dark' : 'light';
    document.documentElement.dataset.theme = next;
    try { localStorage.setItem('mariana-docs-theme', next); } catch (e) { /* private mode */ }
  });

  // ---- Sidebar ------------------------------------------------------------------------------
  document.querySelectorAll('.nav-group-title').forEach((button) => {
    button.addEventListener('click', () => button.parentElement.classList.toggle('open'));
  });
  const menu = document.querySelector('.menu');
  menu && menu.addEventListener('click', () => document.body.classList.toggle('nav-open'));
  const active = document.querySelector('.sidebar a.active');
  if (active) active.scrollIntoView({ block: 'center' });

  // ---- Copy buttons -------------------------------------------------------------------------
  document.querySelectorAll('.code-block .copy').forEach((button) => {
    button.addEventListener('click', async () => {
      const code = button.closest('.code-block').querySelector('pre').innerText;
      try {
        await navigator.clipboard.writeText(code);
      } catch (e) {
        const area = document.createElement('textarea');
        area.value = code;
        document.body.appendChild(area);
        area.select();
        document.execCommand('copy');
        area.remove();
      }
      button.textContent = 'Copied';
      button.classList.add('done');
      setTimeout(() => { button.textContent = 'Copy'; button.classList.remove('done'); }, 1400);
    });
  });

  // ---- Images -------------------------------------------------------------------------------
  const lightbox = document.querySelector('.lightbox');
  document.querySelectorAll('figure a.zoom, .hero-shot a').forEach((link) => {
    link.addEventListener('click', (event) => {
      event.preventDefault();
      lightbox.querySelector('img').src = link.getAttribute('href');
      lightbox.hidden = false;
    });
  });
  lightbox && lightbox.addEventListener('click', () => { lightbox.hidden = true; });

  // ---- On this page: the heading in view ----------------------------------------------------
  const tocLinks = Array.from(document.querySelectorAll('.toc a'));
  if (tocLinks.length) {
    const targets = tocLinks.map((a) => document.getElementById(a.getAttribute('href').slice(1))).filter(Boolean);
    const update = () => {
      let current = targets[0];
      for (const t of targets) if (t.getBoundingClientRect().top < 120) current = t;
      tocLinks.forEach((a) => a.classList.toggle('current', current && a.getAttribute('href') === '#' + current.id));
    };
    document.addEventListener('scroll', update, { passive: true });
    update();
  }

  // ---- Search -------------------------------------------------------------------------------
  const index = window.DOCS_INDEX || [];
  const input = document.querySelector('.search input');
  const results = document.querySelector('.search-results');
  if (!input) return;

  const escape = (s) => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const mark = (text, words) => {
    let out = escape(text);
    for (const w of words) {
      if (w.length < 2) continue;
      out = out.replace(new RegExp('(' + w.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'ig'), '<mark>$1</mark>');
    }
    return out;
  };

  function search(query) {
    const words = query.toLowerCase().split(/\s+/).filter(Boolean);
    if (!words.length) return [];
    const hits = [];
    for (const page of index) {
      const title = page.title.toLowerCase();
      const text = page.text.toLowerCase();
      let score = 0;
      let ok = true;
      for (const w of words) {
        let s = 0;
        if (title.includes(w)) s += title.startsWith(w) ? 30 : 18;
        for (const h of page.headings) if (h.text.toLowerCase().includes(w)) { s += 8; break; }
        if (page.description.toLowerCase().includes(w)) s += 5;
        const count = text.split(w).length - 1;
        s += Math.min(count, 10);
        if (!s) { ok = false; break; }
        score += s;
      }
      if (!ok) continue;
      // The best heading to land on.
      let anchor = '';
      for (const h of page.headings) {
        const t = h.text.toLowerCase();
        if (words.every((w) => t.includes(w))) { anchor = h.id; break; }
      }
      if (!anchor) for (const h of page.headings) if (h.text.toLowerCase().includes(words[0])) { anchor = h.id; break; }
      // A snippet around the first match.
      const at = text.indexOf(words[0]);
      const start = Math.max(0, at - 60);
      const snippet = at >= 0 ? (start > 0 ? '…' : '') + page.text.slice(start, at + 110) + '…' : page.description;
      hits.push({ page, score, anchor, snippet, words });
    }
    return hits.sort((a, b) => b.score - a.score).slice(0, 12);
  }

  let selected = -1;
  function render(hits) {
    if (!input.value.trim()) { results.hidden = true; return; }
    results.hidden = false;
    if (!hits.length) { results.innerHTML = '<div class="empty">Nothing found for “' + escape(input.value) + '”</div>'; return; }
    results.innerHTML = hits.map((h, i) => {
      const href = root + h.page.id + '.html' + (h.anchor ? '#' + h.anchor : '');
      const heading = h.anchor ? h.page.headings.find((x) => x.id === h.anchor) : null;
      return '<a href="' + href + '"' + (i === selected ? ' class="selected"' : '') + '>' +
        '<div><span class="r-title">' + mark(h.page.title, h.words) + '</span>' +
        '<span class="r-section">' + escape(h.page.section) + (heading ? ' › ' + escape(heading.text) : '') + '</span></div>' +
        '<div class="r-snippet">' + mark(h.snippet, h.words) + '</div></a>';
    }).join('');
  }

  let hits = [];
  input.addEventListener('input', () => { selected = -1; hits = search(input.value); render(hits); });
  input.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowDown') { selected = Math.min(selected + 1, hits.length - 1); render(hits); e.preventDefault(); }
    else if (e.key === 'ArrowUp') { selected = Math.max(selected - 1, 0); render(hits); e.preventDefault(); }
    else if (e.key === 'Enter') {
      const pick = hits[Math.max(selected, 0)];
      if (pick) location.href = root + pick.page.id + '.html' + (pick.anchor ? '#' + pick.anchor : '');
    } else if (e.key === 'Escape') { input.value = ''; results.hidden = true; input.blur(); }
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === '/' && document.activeElement !== input && !/input|textarea/i.test(document.activeElement.tagName)) {
      e.preventDefault();
      input.focus();
    }
  });
  document.addEventListener('click', (e) => { if (!e.target.closest('.search')) results.hidden = true; });
})();
