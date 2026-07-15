(() => {
  "use strict";

  const dataElement = document.getElementById("wiki-data");
  if (!dataElement) return;

  const data = JSON.parse(dataElement.textContent);
  const pages = data.pages;
  const byId = new Map(pages.map((page) => [page.id, page]));
  const main = document.getElementById("wiki-main");
  const nav = document.getElementById("wiki-nav");
  const search = document.getElementById("wiki-search");
  const sidebar = document.getElementById("wiki-sidebar");
  const navToggle = document.getElementById("nav-toggle");

  const escapeHtml = (value) => String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");

  const textFromHtml = (value) => {
    const holder = document.createElement("div");
    holder.innerHTML = value;
    return holder.textContent || "";
  };

  pages.forEach((page) => {
    page.searchText = [page.title, page.summary, page.section, ...page.tags, textFromHtml(page.html)]
      .join(" ")
      .toLocaleLowerCase();
  });

  const route = () => {
    const raw = decodeURIComponent(location.hash.replace(/^#/, ""));
    const [id, heading = ""] = raw.split("/", 2);
    return { id, heading };
  };

  const visiblePages = () => {
    const query = search.value.trim().toLocaleLowerCase();
    return pages.filter((page) => !query || page.searchText.includes(query));
  };

  const renderNav = (activeId = "") => {
    const visible = new Set(visiblePages().map((page) => page.id));
    nav.innerHTML = data.sections.map((section) => {
      const links = pages.filter((page) => page.section === section && visible.has(page.id));
      if (!links.length) return "";
      return `<section class="nav-section"><h2>${escapeHtml(section)}</h2>${links.map((page) =>
        `<a href="#${encodeURIComponent(page.id)}" class="${page.id === activeId ? "active" : ""}">${escapeHtml(page.title)}</a>`
      ).join("")}</section>`;
    }).join("");
  };

  const renderHome = () => {
    const visible = visiblePages();
    document.title = "SagaWiki — SagaEngine knowledge base";
    main.innerHTML = `<section class="home">
      <header class="home-header">
        <h1>SagaWiki</h1>
        <p>Current SagaEngine architecture, contracts, workflows, and boundaries.</p>
      </header>
      <div class="document-index">${data.sections.map((section) => {
        const sectionPages = visible.filter((page) => page.section === section);
        if (!sectionPages.length) return "";
        return `<section class="document-section"><h2>${escapeHtml(section)}</h2><ul>${sectionPages.map((page) => `<li><a href="#${encodeURIComponent(page.id)}"><strong>${escapeHtml(page.title)}</strong><span>${escapeHtml(page.summary)}</span></a></li>`).join("")}</ul></section>`;
      }).join("")}</div>
      ${visible.length ? "" : '<p class="empty-state">No article matches your search.</p>'}
    </section>`;
  };

  const renderArticle = (page, heading) => {
    document.title = `${page.title} — SagaWiki`;
    const history = page.source_docs.length
      ? `<details class="history-sources"><summary>Historical inputs synthesized into this article (${page.source_docs.length})</summary><ul>${page.source_docs.map((path) => `<li>${escapeHtml(path)}</li>`).join("")}</ul></details>`
      : "";
    const toc = page.toc.length
      ? `<aside class="toc" aria-label="On this page"><h2>On this page</h2>${page.toc.map((item) => `<a data-level="${item.level}" href="#${encodeURIComponent(page.id)}/${encodeURIComponent(item.slug)}">${escapeHtml(item.title)}</a>`).join("")}</aside>`
      : "";
    main.innerHTML = `<div class="article-layout">
      <article>
        <header class="article-header">
          <p class="article-kicker">${escapeHtml(page.section)} · ${escapeHtml(page.status)}</p>
          <h1>${escapeHtml(page.title)}</h1>
          <p class="article-summary">${escapeHtml(page.summary)}</p>
          <div class="article-actions">
            <button id="rendered-view" class="action-button active" type="button">Article</button>
            <button id="source-view" class="action-button" type="button">Markdown</button>
            <a class="action-button" href="${escapeHtml(page.source)}">Source file</a>
          </div>
        </header>
        <div id="article-content" class="article-body">${page.html}</div>
        ${history}
      </article>
      ${toc}
    </div>`;

    const content = document.getElementById("article-content");
    const readableButton = document.getElementById("rendered-view");
    const sourceButton = document.getElementById("source-view");
    readableButton.addEventListener("click", () => {
      content.className = "article-body";
      content.innerHTML = page.html;
      readableButton.classList.add("active");
      sourceButton.classList.remove("active");
    });
    sourceButton.addEventListener("click", () => {
      content.className = "source-view";
      content.innerHTML = `<p class="source-note">Canonical source: ${escapeHtml(page.source)}</p><pre><code>${escapeHtml(page.markdown)}</code></pre>`;
      sourceButton.classList.add("active");
      readableButton.classList.remove("active");
    });
    if (heading) requestAnimationFrame(() => document.getElementById(heading)?.scrollIntoView());
  };

  const render = () => {
    const current = route();
    const page = byId.get(current.id);
    renderNav(page?.id || "");
    if (page) renderArticle(page, current.heading);
    else renderHome();
    sidebar.classList.remove("open");
    navToggle.setAttribute("aria-expanded", "false");
  };

  search.addEventListener("input", () => {
    if (route().id) history.replaceState(null, "", location.pathname + location.search);
    render();
  });
  navToggle.addEventListener("click", () => {
    const open = sidebar.classList.toggle("open");
    navToggle.setAttribute("aria-expanded", String(open));
  });
  window.addEventListener("hashchange", render);
  document.addEventListener("keydown", (event) => {
    if (event.key === "/" && document.activeElement !== search) {
      event.preventDefault();
      search.focus();
    }
    if (event.key === "Escape") {
      search.blur();
      sidebar.classList.remove("open");
      navToggle.setAttribute("aria-expanded", "false");
    }
  });

  render();
})();
