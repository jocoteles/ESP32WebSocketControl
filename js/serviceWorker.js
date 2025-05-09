 // js/service-worker.js

 const CACHE_NAME = 'esp32-control-cache-v1'; // Cache version, change to update cache
 // List of files that make up the "app shell" to be cached
 const urlsToCache = [
     '.', // Alias for index.html (or your start_url from manifest)
     'index.html',
     'css/pico.min.css',
     'css/styles.css',
     'js/main.js',
     'js/websocketService.js',
     'js/uiUpdater.js',
     'js/appState.js',
     'icons/icon-192x192.png',
     'icons/icon-512x512.png',
     'icons/icon-1024x1024.png'
 ];

 // --- Service Worker Lifecycle Events ---

 // Install event: Cache the app shell
 self.addEventListener('install', (event) => {
     console.log('Service Worker: Installing...');
     event.waitUntil(
         caches.open(CACHE_NAME)
             .then((cache) => {
                 console.log('Service Worker: Caching app shell');
                 return cache.addAll(urlsToCache);
             })
             .then(() => {
                 console.log('Service Worker: App shell cached successfully.');
                 return self.skipWaiting(); // Activate new SW immediately
             })
             .catch((error) => {
                 console.error('Service Worker: Failed to cache app shell:', error);
             })
     );
 });

 // Activate event: Clean up old caches
 self.addEventListener('activate', (event) => {
     console.log('Service Worker: Activating...');
     event.waitUntil(
         caches.keys().then((cacheNames) => {
             return Promise.all(
                 cacheNames.map((cacheName) => {
                     if (cacheName !== CACHE_NAME) {
                         console.log('Service Worker: Deleting old cache:', cacheName);
                         return caches.delete(cacheName);
                     }
                 })
             );
         }).then(() => {
             console.log('Service Worker: Activated and old caches cleaned.');
             return self.clients.claim(); // Take control of uncontrolled clients
         })
     );
 });

 // Fetch event: Serve assets from cache if available (Cache-First strategy for app shell)
 self.addEventListener('fetch', (event) => {
     // We only want to cache GET requests for our app shell assets
     if (event.request.method !== 'GET') {
         return;
     }

     // For app shell assets, try cache first
     // For other requests (like WebSocket or external APIs), go to network
     // This simple example will try to cache all GET requests matched by urlsToCache
     event.respondWith(
         caches.match(event.request)
             .then((response) => {
                 if (response) {
                     // Found in cache, return it
                     // console.log('Service Worker: Serving from cache:', event.request.url);
                     return response;
                 }
                 // Not found in cache, fetch from network
                 // console.log('Service Worker: Fetching from network:', event.request.url);
                 return fetch(event.request).then(
                     (networkResponse) => {
                         // Optionally, cache new requests on the fly if they are part of your app
                         // Be careful with this for dynamic content or external resources.
                         // For this example, we primarily rely on the install-time caching.
                         return networkResponse;
                     }
                 ).catch(error => {
                     console.error('Service Worker: Fetch failed for:', event.request.url, error);
                     // You could return a custom offline page here if desired for non-cached assets
                     // For example: return caches.match('offline.html');
                 });
             })
     );
 });