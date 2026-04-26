import { useMutation, useQueryClient } from '@tanstack/react-query';
import { deleteMedia } from '@/app/api/mediaroot-server-api';

const useDeleteMedia = () => {
  const queryClient = useQueryClient();
  return useMutation<{ result: string }, Error, number>({
    mutationFn: (id: number) => deleteMedia(id),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['media'] });   //refresh library after delete
    },
  });
};

export { useDeleteMedia };