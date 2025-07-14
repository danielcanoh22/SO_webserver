document.addEventListener("DOMContentLoaded", () => {
  const getUriInput = document.getElementById("get-uri");
  const getBtn = document.getElementById("get-btn");
  const getResultsContainer = document.getElementById("get-results");

  const fetchResource = async (uri) => {
    const resultBox = document.createElement("div");
    resultBox.className = "result-box";

    resultBox.innerHTML = `<h4>Petición GET a: <code>${uri}</code></h4><pre>Cargando...</pre>`;
    getResultsContainer.prepend(resultBox);

    try {
      const response = await fetch(uri);
      const contentType = response.headers.get("Content-Type");
      const contentContainer = resultBox.querySelector("pre");

      if (contentType && contentType.startsWith("image/")) {
        const imageBlob = await response.blob();
        const imageObjectURL = URL.createObjectURL(imageBlob);

        const img = document.createElement("img");
        img.src = imageObjectURL;
        img.style.maxWidth = "100%";
        contentContainer.innerHTML = "";
        contentContainer.appendChild(img);
      } else {
        const data = await response.text();
        contentContainer.textContent = data;
      }
    } catch (error) {
      resultBox.querySelector("pre").textContent = `Error: ${error}`;
    }
  };

  getBtn.addEventListener("click", () => fetchResource(getUriInput.value));

  const postForm = document.getElementById("post-form");
  const postResultContainer = document.getElementById("post-result");

  postForm.addEventListener("submit", async (event) => {
    event.preventDefault();

    const formData = new FormData(postForm);
    const data = new URLSearchParams(formData).toString();

    postResultContainer.innerHTML = `<div class="result-box"><h4>Enviando POST a <code>/spin.cgi?2</code>...</h4><pre>Cargando...</pre></div>`;

    try {
      const response = await fetch("/spin.cgi?2", {
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded",
        },
        body: data,
      });
      const responseData = await response.text();
      postResultContainer.querySelector("pre").innerHTML = responseData;
    } catch (error) {
      postResultContainer.querySelector("pre").textContent = `Error: ${error}`;
    }
  });

  const concurrencyBtn = document.getElementById("concurrency-btn");
  const clearBtn = document.getElementById("clear-btn");
  const concurrencyResultsContainer = document.getElementById(
    "concurrency-results"
  );

  const requests = [
    { uri: "/spin.cgi?10", type: "Lenta (CGI)" },
    { uri: "/another.html", type: "Rápida (HTML)" },
    { uri: "/file.txt", type: "Rápida (Texto)" },
    { uri: "/spin.cgi?5", type: "Media (CGI)" },
    { uri: "/no_existe.html", type: "Rápida (Error 404)" },
    { uri: "/another.html", type: "Rápida (HTML)" },
  ];

  const launchRequest = async (req, resultBox) => {
    try {
      const response = await fetch(req.uri);
      const data = await response.text();

      const timestamp = new Date().toLocaleTimeString();
      resultBox.querySelector(
        ".timestamp"
      ).textContent = `Completada a las ${timestamp}`;

      if (response.ok) {
        resultBox.classList.add("success");
        resultBox.querySelector("pre").textContent = data;
      } else {
        resultBox.classList.add("error");
        resultBox.querySelector(
          "pre"
        ).textContent = `Error ${response.status}: ${response.statusText}\n\n${data}`;
      }
    } catch (error) {
      resultBox.classList.add("error");
      resultBox.querySelector("pre").textContent = `Error de red: ${error}`;
    }
  };

  concurrencyBtn.addEventListener("click", () => {
    concurrencyResultsContainer.innerHTML = "";

    requests.forEach((req) => {
      const resultBox = document.createElement("div");
      resultBox.className = "result-box";

      const timestamp = new Date().toLocaleTimeString();
      resultBox.innerHTML = `
                <h4>${req.type} <code>${req.uri}</code> <span class="timestamp">Iniciada a las ${timestamp}</span></h4>
                <pre>Procesando...</pre>
            `;
      concurrencyResultsContainer.appendChild(resultBox);

      launchRequest(req, resultBox);
    });
  });

  clearBtn.addEventListener("click", () => {
    concurrencyResultsContainer.innerHTML = "";
  });
});
