FROM node:18-alpine
WORKDIR /usr/src/app
COPY package*.json ./
RUN npm install --only=production --no-audit --no-fund
COPY . .
EXPOSE 3000
CMD ["node", "src/server.js"]
