import SideMenu from '@/app/components/dashboard/side-menu';
import MediaList from '@/app/components/dashboard/media-list';
import Options from '@/app/components/dashboard/options';
import Grid from '@mui/material/Grid';

export default function Home() {
  return (
    <main>
      <Grid container spacing={3} sx={{ ml: 2, mr: 2, mb: 1, mt: 1}}>
        <Grid size={4} sx={{minWidth: 210}}>
          <Options />
          <SideMenu />
        </Grid>
        <Grid size='grow' sx={{minWidth: 200}}>
          <MediaList />
        </Grid>
      </Grid>
    </main>
  );
}
