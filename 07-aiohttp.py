from aiohttp import web

counter = 0

async def index(request):
    global counter
    counter += 1
    return web.Response(text=str(counter))


app = web.Application()
app.add_routes([web.get('/', index)])

web.run_app(app, port=5000)
