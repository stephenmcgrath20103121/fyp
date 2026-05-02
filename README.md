# MediaRoot - FYP Code Repository
## A Client-Server Based Media Centre Based on Ease of Use

### Build and Installation Guide - Linux
***This method has been successful based on tests performed on Arch Linux and Fedora Linux. Your mileage may vary.***

First, clone the repository using `git clone`, or download a zip archive of it. The following dependencies must be installed: 

Server: 
- GCC (or another C++ compiler eg. Clang)
- CMake
- FFmpeg
- POCO\* (Net, Util, Foundation, JSON)
- SQLite3\*

\* May also require "devel" or similarly named packages, depending on your distribution

Client: 
- Node.js
- npm
- pnpm (install through npm)

The simplest way to do so is with your distribution's package manager. For example, on Debian-based distributions inputting the command `sudo apt install nodejs` in a terminal will install nodejs.

#### Server
Open a terminal and navigate to the project folder, then to the directory server/. Run the following commands: 

`cmake -B build`

`cmake --build build --parallel`

The server binary will be located in the subdirectory build/ , named "mediaroot_server". Move this binary to any folder you desire on your device, for example Documents. To execute it, navigate to the chosen directory in a terminal and type "./mediaroot_server". The server will now be listening both on http://localhost:8080/ and port 8080 of your device's IP address on your local network. To shut down the server, input the key combination "ctrl + c" in the terminal you used to start it. 

#### Client
Open a terminal and navigate to the project folder, then to the directory client/. Create the file ".env.local" and enter the line "NEXT_PUBLIC_MEDIA_API=***chosen server address***", replacing ***chosen server address*** with either the localhost or local network address for the server. This information is outputted in the terminal you started the server in. Save it, then run the following commands: 

`pnpm install`

`pnpm run build`

`pnpm run start`

The client will now be listening on http://localhost:3000/ . Input the URL into your web browser to display the application. To shut down the client, input the key combination "ctrl + c" in the terminal you used to start it. 

### Information
This repository contains two separate programs, divided into the "client" and "server" folders. Currently, only video is fully implemented. 

#### Client
The client is a Next.js + React web app. It uses TypeScript, ESLint, React Compiler and App Router. It also uses the following npm packages: 
- @emotion/react 
- @emotion/styled 
- @emotion/server
- @emotion/cache 
- HLS.js 
- @mui/material 
- @mui/material-nextjs 
- @mui/icons-material
- plyr 
- @tanstack/react-query
- @tanstack/react-query-devtools 

By default it runs on http://localhost:3000/. Users can add media from their client's file browser, which gets displayed in the list of media on the dashboard. The View button displays an added media file on a separate page, the Edit button allows the user to change the displayed title and the Delete button removes the media from the server, along with all related cache files, playlist files and thumbnails. The side menu filters media based on its file type. The search bar can be used to filter any media currently in the list by their title. The media list also features pagination. 

#### Server
The server is a REST server made in C++, along with FFmpeg and the POCO libraries. It is compiled with CMake. It provides an API to the client and uses a SQLite database for data storage. By default, it runs on http://localhost:8080/.

The following main capabilites are currently implemented: 
- CRUD operations for media stored on the server
- Stream added videos and audio
- Retrieve added images
- Retrieve video thumbnails