FROM alpine:3.22@sha256:5291449c3df73caf6ed85e649dec1b9e818b39a5d8c871e97afc13e9cd5e8fa8 AS build
RUN apk add --no-cache gcc musl-dev make pkgconf cjson-dev cjson-static tzdata tar gzip
WORKDIR /src
COPY Makefile ./
COPY src ./src
COPY tests ./tests
COPY packaging/licenses ./licenses
ARG VERSION
RUN printf '%s\n' "$VERSION" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+$' && \
    make test CFLAGS='-std=c11 -Os -Wall -Wextra -Werror -static' && \
    mkdir -p /package /out && \
    cp himalaya-mcp /package/ && \
    cp -R /usr/share/zoneinfo /package/zoneinfo && \
    cp -R licenses /package/licenses && \
    apk info -v musl cjson tzdata > /package/BUILD-DEPENDENCIES.txt && \
    tar -czf "/out/himalaya-mcp-$VERSION-linux-$(uname -m).tar.gz" -C /package .
FROM scratch AS artifact
COPY --from=build /out/ /
