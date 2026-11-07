FROM node:22.14.0-bookworm-slim AS build
WORKDIR /web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web ./
ARG VITE_NANO_SERVER_URL=http://localhost:8080
ENV VITE_NANO_SERVER_URL=$VITE_NANO_SERVER_URL
RUN npm run build

FROM nginx:1.27.4-alpine
COPY --from=build /web/dist /usr/share/nginx/html
COPY docker/nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD wget -q -O /dev/null http://127.0.0.1/ || exit 1
