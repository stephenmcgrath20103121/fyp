import Image from "next/image";
import SideMenu from '@/app/components/dashboard/side-menu';
import MediaList from '@/app/components/dashboard/media-list';
import Box from '@mui/material/Box';
import Grid from '@mui/material/Grid';
import Avatar from '@mui/material/Avatar';
import HomeIcon from '@mui/icons-material/Home';
import SettingsIcon from '@mui/icons-material/Settings';
import IconButton from "@mui/material/IconButton";
import Link from "next/link";

export default function Home() {
  return (
    <main>
      <Grid container>
        <Box sx={{ml: 3, mt: 1, mr: 1, mb: 1}}>
          <Link href={`/media/1`}>
            <IconButton size="small" sx={{bgcolor: "secondary.main", color: "primary.contrastText", m: 1}}>
              <HomeIcon />
            </IconButton>
            <IconButton size="small" sx={{bgcolor: "secondary.main", color: "primary.contrastText", m: 1}}>
              <SettingsIcon />
            </IconButton>
            <SideMenu />
          </Link>
        </Box>
        <Box sx={{ml: 1, mt: 1, mr: 3, mb: 1}}>
          <MediaList />
        </Box>
      </Grid>
    </main>
  );
}
