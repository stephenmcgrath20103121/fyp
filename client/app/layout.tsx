import type { Metadata } from "next";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";
import { AppRouterCacheProvider } from '@mui/material-nextjs/v15-appRouter';
import { ThemeProvider } from '@mui/material/styles';
import Providers from '@/app/providers';
import theme from '@/app/theme';

const geistSans = Geist({
  weight: ['300', '400', '500', '700'],
  variable: "--font-geist-sans",
  display: 'swap',
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  weight: ['300', '400', '500', '700'],
  variable: "--font-geist-mono",
  display: 'swap',
  subsets: ["latin"],
});

export const metadata: Metadata = {
  title: "MediaRoot Client",
  description: "To be used with the MediaRoot Server",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html
      lang="en"
      className={`${geistSans.variable} ${geistMono.variable} h-full antialiased`}
    >
      <body className="min-h-full flex flex-col">
        <AppRouterCacheProvider options={{ enableCssLayer: true }}>
          <ThemeProvider theme={theme}>
            <Providers>{children}</Providers>
          </ThemeProvider>
        </AppRouterCacheProvider>
      </body>
    </html>
  );
}
