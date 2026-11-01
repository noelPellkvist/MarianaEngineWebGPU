#!/usr/bin/env python3
"""Builds the Mariana Engine documentation website.

    python docs/build.py            # writes docs/site/
    python docs/build.py --out DIR  # somewhere else

The pages are Markdown files in docs/pages/ (with a small header: title and description), the
navigation is docs/nav.json, images live in docs/images/ and the theme in docs/theme/. The output is
a static site (plain HTML, CSS and JavaScript, nothing to install and no server code): open
site/index.html or upload the folder anywhere. Only Python's standard library is needed.

Markdown supported: headings, paragraphs, **bold**, *italic*, `code`, links (to other pages as
`page.md`), images (a lone image with a title becomes a figure with a caption), lists (nested),
tables, fenced code (```luau title="Snake.luau"), callouts (> [!NOTE], [!TIP], [!WARNING],
[!IMPORTANT]), horizontal rules and raw HTML blocks.
"""
import argparse
import html
import json
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
PAGES = os.path.join(HERE, 'pages')
IMAGES = os.path.join(HERE, 'images')
THEME = os.path.join(HERE, 'theme')
SITE_NAME = 'Mariana Engine'

# ---------------------------------------------------------------------------------------------
# Syntax highlighting
# ---------------------------------------------------------------------------------------------

LUAU_KEYWORDS = {
    'and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for', 'function', 'if', 'in', 'local',
    'nil', 'not', 'or', 'repeat', 'return', 'then', 'true', 'until', 'while', 'continue', 'type',
    'export', 'typeof',
}
LUAU_BUILTINS = {
    'scene', 'input', 'time', 'physics', 'vec3', 'quat', 'vector', 'math', 'string', 'table',
    'print', 'warn', 'pairs', 'ipairs', 'tostring', 'tonumber', 'pcall', 'error', 'select',
    'setmetatable', 'getmetatable', 'coroutine', 'bit32', 'utf8', 'buffer', 'os', 'self',
    'assert', 'next', 'rawget', 'rawset', 'rawequal', 'unpack',
}
LUAU_TYPES = {
    'Script', 'Entity', 'Component', 'Contact', 'RaycastHit', 'QueryOptions', 'BodyInfo',
    'VehicleInfo', 'CharacterInfo', 'WheelInfo', 'number', 'boolean', 'any', 'never', 'unknown',
}
CPP_KEYWORDS = {
    'auto', 'bool', 'break', 'case', 'class', 'const', 'constexpr', 'continue', 'default', 'delete',
    'do', 'double', 'else', 'enum', 'false', 'float', 'for', 'if', 'include', 'inline', 'int',
    'namespace', 'new', 'nullptr', 'private', 'protected', 'public', 'return', 'static', 'struct',
    'switch', 'template', 'this', 'true', 'typename', 'uint32_t', 'uint64_t', 'using', 'virtual',
    'void', 'while', 'override', 'std',
}
WGSL_KEYWORDS = {
    'fn', 'let', 'var', 'const', 'return', 'if', 'else', 'for', 'loop', 'break', 'continue',
    'struct', 'true', 'false', 'f32', 'i32', 'u32', 'bool', 'vec2f', 'vec3f', 'vec4f', 'vec2',
    'vec3', 'vec4', 'mat4x4f', 'texture_2d', 'sampler', 'override', 'switch', 'case', 'default',
}
SHELL_KEYWORDS = {'cd', 'export', 'set', 'if', 'then', 'fi', 'for', 'do', 'done', 'echo'}
CMAKE_KEYWORDS = {'set', 'if', 'endif', 'else', 'add_subdirectory', 'target_compile_definitions'}

TOKEN = re.compile(r'''
    (?P<lcomment>--\[(?P<eq>=*)\[.*?\](?P=eq)\])     |
    (?P<lstring>\[(?P<eq2>=*)\[.*?\](?P=eq2)\])       |
    (?P<dcomment>--[^\n]*)                           |
    (?P<scomment>//[^\n]*)                           |
    (?P<bcomment>/\*.*?\*/)                          |
    (?P<hcomment>\#[^\n]*)                           |
    (?P<string>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'|`(?:\\.|[^`\\])*`) |
    (?P<number>\b(?:0x[0-9a-fA-F_]+|\d[\d_]*(?:\.\d+)?(?:[eE][-+]?\d+)?)\b) |
    (?P<word>[A-Za-z_][A-Za-z0-9_]*)                  |
    (?P<other>.)
''', re.S | re.X)


def highlight(code, language):
    language = (language or '').lower()
    if language in ('lua', 'luau'):
        keywords, builtins, types = LUAU_KEYWORDS, LUAU_BUILTINS, LUAU_TYPES
        comments = {'lcomment', 'dcomment'}
    elif language in ('cpp', 'c++', 'c', 'hpp'):
        keywords, builtins, types = CPP_KEYWORDS, set(), set()
        comments = {'scomment', 'bcomment'}
    elif language == 'wgsl':
        keywords, builtins, types = WGSL_KEYWORDS, set(), set()
        comments = {'scomment', 'bcomment'}
    elif language in ('bash', 'sh', 'shell', 'powershell', 'ps', 'console'):
        keywords, builtins, types = SHELL_KEYWORDS, set(), set()
        comments = {'hcomment'}
    elif language == 'cmake':
        keywords, builtins, types = CMAKE_KEYWORDS, set(), set()
        comments = {'hcomment'}
    elif language in ('json', 'jsonc'):
        keywords, builtins, types = {'true', 'false', 'null'}, set(), set()
        comments = {'scomment'}
    else:
        return html.escape(code)

    out = []
    pos = 0
    previous_word = ''
    while pos < len(code):
        m = TOKEN.match(code, pos)
        kind = m.lastgroup
        text = m.group(0)
        pos = m.end()
        if kind in ('eq', 'eq2'):
            kind = 'lcomment' if m.group('lcomment') else 'lstring'
        if kind in ('lcomment', 'dcomment', 'scomment', 'bcomment', 'hcomment'):
            if kind in comments:
                out.append(f'<span class="tok-comment">{html.escape(text)}</span>')
                continue
            # Not a comment in this language: take one character and go on.
            text = code[m.start()]
            pos = m.start() + 1
            kind = 'other'
        if kind == 'lstring' and language not in ('lua', 'luau'):
            text = code[m.start()]
            pos = m.start() + 1
            kind = 'other'
        if kind in ('string', 'lstring'):
            if language in ('json', 'jsonc') and code[pos:pos + 20].lstrip().startswith(':'):
                out.append(f'<span class="tok-key">{html.escape(text)}</span>')
            else:
                out.append(f'<span class="tok-string">{html.escape(text)}</span>')
        elif kind == 'number':
            out.append(f'<span class="tok-number">{html.escape(text)}</span>')
        elif kind == 'word':
            following = code[pos:pos + 1]
            if text in keywords:
                cls = 'tok-keyword'
            elif text in types or (language in ('lua', 'luau') and previous_word == ':' and text[:1].isupper()):
                cls = 'tok-type'
            elif text in builtins:
                cls = 'tok-builtin'
            elif following == '(' or (language in ('lua', 'luau') and following in ('"', '{')):
                cls = 'tok-function'
            elif text[:1].isupper() and language in ('lua', 'luau'):
                cls = 'tok-type'
            else:
                cls = ''
            out.append(f'<span class="{cls}">{html.escape(text)}</span>' if cls else html.escape(text))
        else:
            out.append(html.escape(text))
        if kind == 'word':
            previous_word = text
        elif not text.isspace():
            previous_word = text
    return ''.join(out)


# ---------------------------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------------------------

def slugify(text):
    text = re.sub(r'<[^>]+>', '', text)
    text = html.unescape(text).lower()
    text = re.sub(r'[`*_()\[\]{}:,.?!"\'/\\=<>|~]+', '', text)
    text = re.sub(r'\s+', '-', text.strip())
    return re.sub(r'-+', '-', text) or 'section'


class Context:
    """What rendering a page needs: its path (for links) and the headings found."""

    def __init__(self, page_id, root):
        self.page_id = page_id
        self.root = root  # relative path from the page to the site root ('' or '../')
        self.headings = []  # (level, id, text)
        self.ids = set()

    def unique_id(self, base):
        candidate, n = base, 2
        while candidate in self.ids:
            candidate = f'{base}-{n}'
            n += 1
        self.ids.add(candidate)
        return candidate


def fix_link(url, ctx):
    if re.match(r'^[a-z]+:', url) or url.startswith('#') or url.startswith('/'):
        return url
    path, _, anchor = url.partition('#')
    if path.endswith('.md'):
        # Links between pages are written relative to docs/pages/; make them relative to this page.
        target = os.path.normpath(os.path.join(os.path.dirname(ctx.page_id), path[:-3])).replace('\\', '/')
        path = ctx.root + target + '.html'
    elif path.startswith('images/'):
        path = ctx.root + path
    return path + ('#' + anchor if anchor else '')


def inline(text, ctx):
    """Inline Markdown: code spans first (kept as they are), then links, images and emphasis."""
    codes = []

    def keep_code(m):
        codes.append(m.group(1))
        return f'\x00{len(codes) - 1}\x00'

    text = re.sub(r'``\s?(.+?)\s?``', keep_code, text)
    text = re.sub(r'`([^`]+)`', keep_code, text)
    # Raw HTML stays; everything else is escaped.
    parts = re.split(r'(<[a-zA-Z/][^>]*>)', text)
    text = ''.join(p if i % 2 else html.escape(p, quote=False) for i, p in enumerate(parts))

    def image(m):
        alt, src, title = m.group(1), m.group(2), m.group(3)
        title_attr = f' title="{html.escape(title)}"' if title else ''
        return f'<img src="{fix_link(src, ctx)}" alt="{html.escape(alt)}"{title_attr} loading="lazy">'

    text = re.sub(r'!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)', image, text)

    def link(m):
        label, url = m.group(1), m.group(2)
        external = re.match(r'^https?:', url)
        extra = ' target="_blank" rel="noopener"' if external else ''
        return f'<a href="{html.escape(fix_link(url, ctx))}"{extra}>{label}</a>'

    text = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)', link, text)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])', r'<em>\1</em>', text)
    text = re.sub(r'(?<![\w])_(?!\s)(.+?)(?<!\s)_(?![\w])', r'<em>\1</em>', text)
    text = text.replace('  \n', '<br>\n')

    def restore(m):
        return f'<code>{html.escape(codes[int(m.group(1))])}</code>'

    return re.sub(r'\x00(\d+)\x00', restore, text)


CALLOUTS = {
    'NOTE': ('note', 'Note'),
    'TIP': ('tip', 'Tip'),
    'WARNING': ('warning', 'Warning'),
    'IMPORTANT': ('important', 'Important'),
    'CAUTION': ('warning', 'Caution'),
}


def render_table(rows, ctx):
    def cells(row):
        row = row.strip()
        if row.startswith('|'):
            row = row[1:]
        if row.endswith('|') and not row.endswith('\\|'):
            row = row[:-1]
        # Split on pipes that aren't escaped and aren't inside code spans.
        out, current, in_code = [], '', False
        i = 0
        while i < len(row):
            c = row[i]
            if c == '\\' and i + 1 < len(row) and row[i + 1] == '|':
                current += '|'
                i += 2
                continue
            if c == '`':
                in_code = not in_code
            if c == '|' and not in_code:
                out.append(current.strip())
                current = ''
            else:
                current += c
            i += 1
        out.append(current.strip())
        return out

    header = cells(rows[0])
    aligns = []
    for spec in cells(rows[1]):
        left, right = spec.startswith(':'), spec.endswith(':')
        aligns.append('center' if left and right else 'right' if right else '')
    body = [cells(r) for r in rows[2:]]

    def cell(tag, text, i):
        style = f' style="text-align:{aligns[i]}"' if i < len(aligns) and aligns[i] else ''
        return f'<{tag}{style}>{inline(text, ctx)}</{tag}>'

    out = ['<div class="table-wrap"><table>', '<thead><tr>']
    out += [cell('th', h, i) for i, h in enumerate(header)]
    out.append('</tr></thead><tbody>')
    for row in body:
        out.append('<tr>' + ''.join(cell('td', c, i) for i, c in enumerate(row)) + '</tr>')
    out.append('</tbody></table></div>')
    return '\n'.join(out)


def render_list(lines, ctx):
    """Nested lists from lines (indentation decides the level)."""
    items = []  # (indent, ordered, text)
    for line in lines:
        m = re.match(r'^(\s*)([-*+]|\d+[.)])\s+(.*)$', line)
        if m:
            number = int(m.group(2)[:-1]) if m.group(2)[0].isdigit() else 1
            items.append([len(m.group(1).replace('\t', '    ')), m.group(2)[0].isdigit(), m.group(3), number])
        elif items:
            items[-1][2] += '\n' + line.strip()

    def build(start, indent):
        out = []
        ordered = items[start][1]
        tag = 'ol' if ordered else 'ul'
        first = items[start][3] if ordered else 1
        out.append(f'<{tag} start="{first}">' if first != 1 else f'<{tag}>')
        i = start
        while i < len(items) and items[i][0] >= indent:
            if items[i][0] > indent:
                sub, i = build(i, items[i][0])
                out[-1] = out[-1][:-5] + sub + '</li>'
                continue
            text = items[i][2]
            # Task list boxes.
            text = re.sub(r'^\[ \]\s', '<span class="task"></span>', text)
            text = re.sub(r'^\[x\]\s', '<span class="task done"></span>', text)
            out.append(f'<li>{inline(text, ctx)}</li>')
            i += 1
        out.append(f'</{tag}>')
        return ''.join(out), i

    html_out, _ = build(0, items[0][0])
    return html_out


def render_code(lines, info):
    language = info.split()[0] if info else ''
    title = ''
    m = re.search(r'title="([^"]*)"', info or '')
    if m:
        title = m.group(1)
    code = '\n'.join(lines)
    label = title or {'luau': 'Luau', 'lua': 'Luau', 'json': 'JSON', 'bash': 'Shell', 'sh': 'Shell',
                      'powershell': 'PowerShell', 'cpp': 'C++', 'wgsl': 'WGSL', 'cmake': 'CMake',
                      'text': 'Text', 'console': 'Console'}.get(language.lower(), language.upper() if language else '')
    head = (f'<div class="code-head"><span class="code-label{" code-file" if title else ""}">{html.escape(label)}</span>'
            f'<button class="copy" type="button" aria-label="Copy code">Copy</button></div>')
    return (f'<div class="code-block" data-lang="{html.escape(language)}">{head}'
            f'<pre><code>{highlight(code, language)}</code></pre></div>')


def render_markdown(text, ctx):
    lines = text.replace('\r\n', '\n').split('\n')
    out = []
    i = 0
    paragraph = []

    def flush_paragraph():
        if not paragraph:
            return
        content = '\n'.join(paragraph).strip()
        paragraph.clear()
        # A lone image with a title: a figure.
        m = re.fullmatch(r'!\[([^\]]*)\]\(([^)\s]+)\s+"([^"]*)"\)', content)
        if m:
            out.append(f'<figure><a class="zoom" href="{fix_link(m.group(2), ctx)}"><img src="{fix_link(m.group(2), ctx)}" '
                       f'alt="{html.escape(m.group(1))}" loading="lazy"></a><figcaption>{inline(m.group(3), ctx)}</figcaption></figure>')
            return
        out.append(f'<p>{inline(content, ctx)}</p>')

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Fenced code.
        m = re.match(r'^(\s*)```(.*)$', line)
        if m:
            flush_paragraph()
            indent = len(m.group(1))
            info = m.group(2).strip()
            body = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                body.append(lines[i][indent:] if lines[i][:indent].isspace() else lines[i])
                i += 1
            i += 1
            # "@include path" (from the repository's root): the file itself, so listings of the
            # project's scripts are always the scripts as they are.
            if len(body) == 1 and body[0].startswith('@include '):
                included = os.path.join(HERE, '..', body[0][len('@include '):].strip())
                with open(included, encoding='utf-8') as f:
                    body = f.read().replace('\r\n', '\n').rstrip('\n').split('\n')
            out.append(render_code(body, info))
            continue

        if not stripped:
            flush_paragraph()
            i += 1
            continue

        # Raw HTML block: until a blank line.
        if re.match(r'^</?(div|section|nav|table|figure|details|summary|aside|p|ul|ol|h[1-6]|span|a|img|br|hr|svg)\b', stripped) or stripped.startswith('<!--'):
            flush_paragraph()
            block = []
            while i < len(lines) and lines[i].strip():
                block.append(lines[i])
                i += 1
            out.append('\n'.join(block))
            continue

        # Headings.
        m = re.match(r'^(#{1,6})\s+(.*?)\s*#*$', stripped)
        if m:
            flush_paragraph()
            level = len(m.group(1))
            title = m.group(2)
            explicit = re.search(r'\s*\{#([\w-]+)\}$', title)
            if explicit:
                title = title[:explicit.start()]
            heading_id = ctx.unique_id(explicit.group(1) if explicit else slugify(title))
            rendered = inline(title, ctx)
            api = ' class="api"' if title.startswith('`') else ''
            if level in (2, 3):
                ctx.headings.append((level, heading_id, re.sub(r'<[^>]+>', '', rendered)))
            if level == 1:
                out.append(f'<h1 id="{heading_id}">{rendered}</h1>')
            else:
                out.append(f'<h{level} id="{heading_id}"{api}><a class="anchor" href="#{heading_id}" aria-hidden="true">#</a>{rendered}</h{level}>')
            i += 1
            continue

        # Horizontal rule.
        if re.fullmatch(r'(-{3,}|\*{3,}|_{3,})', stripped):
            flush_paragraph()
            out.append('<hr>')
            i += 1
            continue

        # Tables.
        if stripped.startswith('|') and i + 1 < len(lines) and re.match(r'^\s*\|?\s*:?-{3,}', lines[i + 1]):
            flush_paragraph()
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append(lines[i])
                i += 1
            out.append(render_table(rows, ctx))
            continue

        # Blockquotes and callouts.
        if stripped.startswith('>'):
            flush_paragraph()
            block = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                block.append(re.sub(r'^\s*>\s?', '', lines[i]))
                i += 1
            m = re.match(r'^\[!(\w+)\]\s*(.*)$', block[0].strip()) if block else None
            if m and m.group(1).upper() in CALLOUTS:
                kind, label = CALLOUTS[m.group(1).upper()]
                title = m.group(2) or label
                inner = render_markdown('\n'.join(block[1:]), ctx)
                out.append(f'<div class="callout {kind}"><div class="callout-title">{inline(title, ctx)}</div>{inner}</div>')
            else:
                out.append(f'<blockquote>{render_markdown(chr(10).join(block), ctx)}</blockquote>')
            continue

        # Lists.
        if re.match(r'^\s*([-*+]|\d+[.)])\s+', line):
            flush_paragraph()
            block = []
            while i < len(lines):
                l = lines[i]
                if re.match(r'^\s*([-*+]|\d+[.)])\s+', l) or (l.strip() and l.startswith((' ', '\t')) and block):
                    if re.match(r'^\s*```', l):
                        break
                    block.append(l)
                    i += 1
                elif not l.strip() and i + 1 < len(lines) and re.match(r'^\s*([-*+]|\d+[.)])\s+', lines[i + 1]) \
                        and len(lines[i + 1]) - len(lines[i + 1].lstrip()) > 0:
                    i += 1
                else:
                    break
            out.append(render_list(block, ctx))
            continue

        paragraph.append(line)
        i += 1

    flush_paragraph()
    return '\n'.join(out)


# ---------------------------------------------------------------------------------------------
# Pages
# ---------------------------------------------------------------------------------------------

def read_page(page_id):
    path = os.path.join(PAGES, page_id + '.md')
    with open(path, encoding='utf-8') as f:
        text = f.read()
    meta = {}
    m = re.match(r'^---\n(.*?)\n---\n', text, re.S)
    if m:
        for line in m.group(1).split('\n'):
            key, _, value = line.partition(':')
            if key.strip():
                meta[key.strip()] = value.strip()
        text = text[m.end():]
    return meta, text


def plain_text(markdown):
    text = re.sub(r'```.*?```', ' ', markdown, flags=re.S)
    text = re.sub(r'<[^>]+>', ' ', text)
    text = re.sub(r'!\[[^\]]*\]\([^)]*\)', ' ', text)
    text = re.sub(r'\[([^\]]+)\]\([^)]*\)', r'\1', text)
    text = re.sub(r'[#>*`|_-]+', ' ', text)
    return re.sub(r'\s+', ' ', text).strip()


ICONS = {
    'rocket': '<path d="M5 15c-1.5 1.5-2 5-2 5s3.5-.5 5-2m9-9a3 3 0 1 1-6 0 3 3 0 0 1 6 0Zm-1.5-6.5C17 3 20.5 3.5 20.5 3.5S21 7 19.5 11.5L13 18l-7-7 6.5-6.5Z"/>',
    'book': '<path d="M4 4.5A2.5 2.5 0 0 1 6.5 2H20v17H6.5A2.5 2.5 0 0 0 4 21.5v-17Zm0 17A2.5 2.5 0 0 1 6.5 19H20v3H6.5"/>',
    'code': '<path d="m8 6-6 6 6 6m8-12 6 6-6 6M14 4l-4 16"/>',
    'cube': '<path d="m12 2 9 5v10l-9 5-9-5V7l9-5Zm0 0v20m9-15-9 5-9-5"/>',
    'build': '<path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.6 2.6-2.4-.6-.6-2.4 2.6-2.6Z"/>',
    'list': '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
}


def page_shell(title, description, body, ctx, nav, page_id, pages_meta, prev_next, section_title):
    root = ctx.root

    # Sidebar.
    side = []
    for group in nav:
        is_open = any(p == page_id for p in group['pages'])
        side.append(f'<div class="nav-group{" open" if is_open else ""}">')
        side.append(f'<button class="nav-group-title" type="button">{html.escape(group["title"])}<span class="chev"></span></button><ul>')
        for p in group['pages']:
            meta = pages_meta[p]
            active = ' class="active" aria-current="page"' if p == page_id else ''
            side.append(f'<li><a{active} href="{root}{p}.html">{html.escape(meta.get("nav", meta.get("title", p)))}</a></li>')
        side.append('</ul></div>')

    # On this page.
    toc = []
    if len(ctx.headings) > 1:
        toc.append('<div class="toc-title">On this page</div><ul>')
        for level, hid, text in ctx.headings:
            toc.append(f'<li class="l{level}"><a href="#{hid}">{html.escape(text)}</a></li>')
        toc.append('</ul>')

    crumbs = ''
    if page_id != 'index':
        crumbs = (f'<nav class="crumbs"><a href="{root}index.html">Docs</a><span>/</span>'
                  f'<span>{html.escape(section_title)}</span></nav>')

    pager = ''
    prev_page, next_page = prev_next
    if prev_page or next_page:
        pager = '<nav class="pager">'
        if prev_page:
            pager += (f'<a class="prev" href="{root}{prev_page}.html"><span>Previous</span>'
                      f'{html.escape(pages_meta[prev_page].get("title", prev_page))}</a>')
        else:
            pager += '<span></span>'
        if next_page:
            pager += (f'<a class="next" href="{root}{next_page}.html"><span>Next</span>'
                      f'{html.escape(pages_meta[next_page].get("title", next_page))}</a>')
        pager += '</nav>'

    full_title = title if page_id == 'index' else f'{title} | {SITE_NAME} Docs'
    wide = ' wide' if page_id == 'index' else ''
    return f'''<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(full_title)}</title>
<meta name="description" content="{html.escape(description)}">
<link rel="icon" href="{root}images/logo.png">
<link rel="stylesheet" href="{root}theme/style.css">
<script>try{{const t=localStorage.getItem('mariana-docs-theme');if(t)document.documentElement.dataset.theme=t;}}catch(e){{}}</script>
</head>
<body class="page-{html.escape(page_id.replace('/', '-'))}{wide}" data-root="{root}">
<header class="topbar">
  <button class="menu" type="button" aria-label="Menu"><span></span></button>
  <a class="brand" href="{root}index.html"><img src="{root}images/logo.png" alt=""><span class="brand-name">Mariana Engine</span><span class="brand-tag">Docs</span></a>
  <nav class="top-links">
    <a href="{root}getting-started/first-game.html">Tutorial</a>
    <a href="{root}manual/editor.html">Manual</a>
    <a href="{root}scripting/overview.html">Scripting</a>
    <a href="{root}scripting/api/index.html">API Reference</a>
    <a href="{root}building/build-profiles.html">Building</a>
  </nav>
  <div class="search">
    <input type="search" placeholder="Search the docs" aria-label="Search the docs" autocomplete="off">
    <kbd>/</kbd>
    <div class="search-results" hidden></div>
  </div>
  <button class="theme" type="button" aria-label="Light or dark"></button>
</header>
<div class="layout">
  <aside class="sidebar"><nav>{"".join(side)}</nav></aside>
  <main class="content">
    {crumbs}
    <article>{body}</article>
    {pager}
    <footer class="footer"><span>Mariana Engine documentation</span><span>Built from <code>docs/pages</code></span></footer>
  </main>
  <aside class="toc">{"".join(toc)}</aside>
</div>
<div class="lightbox" hidden><img alt=""></div>
<script src="{root}search-index.js"></script>
<script src="{root}theme/docs.js"></script>
</body>
</html>
'''


def build(out_dir):
    with open(os.path.join(HERE, 'nav.json'), encoding='utf-8') as f:
        nav = json.load(f)
    order = [p for group in nav for p in group['pages']]
    section_of = {p: group['title'] for group in nav for p in group['pages']}
    pages_meta = {}
    sources = {}
    for page_id in order:
        meta, text = read_page(page_id)
        pages_meta[page_id] = meta
        sources[page_id] = text

    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)
    shutil.copytree(THEME, os.path.join(out_dir, 'theme'))
    if os.path.isdir(IMAGES):
        shutil.copytree(IMAGES, os.path.join(out_dir, 'images'))

    search = []
    for index, page_id in enumerate(order):
        meta, text = pages_meta[page_id], sources[page_id]
        depth = page_id.count('/')
        ctx = Context(page_id, '../' * depth)
        body = render_markdown(text, ctx)
        title = meta.get('title', page_id)
        prev_page = order[index - 1] if index > 0 else None
        next_page = order[index + 1] if index + 1 < len(order) else None
        page = page_shell(title, meta.get('description', ''), body, ctx, nav, page_id, pages_meta,
                          (prev_page, next_page), section_of[page_id])
        target = os.path.join(out_dir, page_id + '.html')
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, 'w', encoding='utf-8', newline='\n') as f:
            f.write(page)
        search.append({
            'id': page_id,
            'title': title,
            'section': section_of[page_id],
            'description': meta.get('description', ''),
            'headings': [{'id': hid, 'text': t} for _, hid, t in ctx.headings],
            'text': plain_text(text)[:6000],
        })

    with open(os.path.join(out_dir, 'search-index.js'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('window.DOCS_INDEX = ' + json.dumps(search, ensure_ascii=False) + ';\n')
    problems = check_links(out_dir, order)
    for problem in problems:
        print('warning: ' + problem)
    print(f'Built {len(order)} pages into {out_dir}' + (f' ({len(problems)} broken links)' if problems else ''))


def check_links(out_dir, order):
    """Every link and image in the built pages that points at a missing page, anchor or file."""
    ids = {}
    for page_id in order:
        with open(os.path.join(out_dir, page_id + '.html'), encoding='utf-8') as f:
            ids[page_id] = set(re.findall(r'\bid="([^"]+)"', f.read()))
    problems = []
    for page_id in order:
        with open(os.path.join(out_dir, page_id + '.html'), encoding='utf-8') as f:
            text = f.read()
        base = os.path.dirname(page_id)
        for url in re.findall(r'\b(?:href|src)="([^"]+)"', text):
            url = html.unescape(url)
            if re.match(r'^[a-z]+:', url):
                continue
            path, _, anchor = url.partition('#')
            target = page_id
            if path:
                target = os.path.normpath(os.path.join(base, path)).replace('\\', '/')
                if not target.endswith('.html'):
                    if not os.path.exists(os.path.join(out_dir, target)):
                        problems.append(f'{page_id}: no file {url}')
                    continue
                target = target[:-len('.html')]
                if target not in ids:
                    problems.append(f'{page_id}: no page {url}')
                    continue
            if anchor and anchor not in ids[target]:
                problems.append(f'{page_id}: no anchor {url}')
    return sorted(set(problems))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--out', default=os.path.join(HERE, 'site'), help='output folder (default docs/site)')
    args = parser.parse_args()
    build(os.path.abspath(args.out))


if __name__ == '__main__':
    main()
